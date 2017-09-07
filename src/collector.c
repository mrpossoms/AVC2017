#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <getopt.h>

#include "structs.h"
#include "dataset_hdr.h"
#include "i2c.h"
#include "cam.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

typedef enum {
	COL_MODE_NORMAL = 0,
	COL_MODE_ACT_CAL,
} col_mode_t;

#define THROTTLE_STOPPED 117
int I2C_BUS;
int NORM_VIDEO;
col_mode_t MODE;
calib_t CAL;

void proc_opts(int argc, const char ** argv)
{
	for(;;)
	{
		int c = getopt(argc, (char *const *)argv, "cn");
		if(c == -1) break;

		switch (c) 
		{
			case 'c':
				MODE = COL_MODE_ACT_CAL;
				fprintf(stderr, "Calibrating action vector\n");
				break;
			case 'n':
				NORM_VIDEO = 1;
				break;

		}
	}
}



int poll_i2c_devs(raw_state_t* state, raw_action_t* action)
{
	if(!action) return 1;

	// Get throttle and steering state
	if(i2c_read(I2C_BUS_FD, PWM_LOGGER_ADDR, 2, (void*)action, sizeof(raw_action_t)))
	{
		return 2;
	}

	uint16_t odo = 0;
	int res = i2c_read(I2C_BUS_FD, PWM_LOGGER_ADDR, 0x0C, (void*)&odo, sizeof(odo));

	fprintf(stderr, "odo: %d\n", odo);

	uint8_t mode = 0;
	res = bno055_get_operation_mode(&mode);

	if(!state) return 3;

	if(bno055_read_accel_xyz((struct bno055_accel_t*)state->acc))
	{
		EXIT("Error reading from BNO055\n");
	}

	if(bno055_read_gyro_xyz((struct bno055_gyro_t*)state->rot_rate))
	{
		EXIT("Error reading from BNO055\n");
	}


	return 0;
}


int poll_vision(raw_state_t* state, cam_t* cams)
{
	cam_wait_frame(cams);

	range_t luma_range = { 128, 128 };
	range_t cr_range = { 128, 128 };
	range_t cb_range = { 128, 128 };
	float luma_mu = 0;
	float cr_mu = 0;
	float cb_mu = 0;


	// Downsample the intensity resolution to match that of
	// the chroma
	for(int j = FRAME_H; j--;)
	{
		uint32_t* row = cams[0].frame_buffer + (j * FRAME_W * 2);
		uint8_t* luma_row = state->view.luma + (j * FRAME_W);
		chroma_t* chroma_row = state->view.chroma + (j * (FRAME_W >> 1));
		
		for(int i = FRAME_W / 2; i--;)
		{
			int li = i << 1;

			luma_row[li + 0] = row[i] & 0xFF;
			luma_row[li + 1] = (row[i] >> 16) & 0xFF;

			chroma_row[i].cr = (row[i] >> 8) & 0xFF;
			chroma_row[i].cb = (row[i] >> 24) & 0xFF;

			if(NORM_VIDEO)
			{
				for(int k = 2; k--;)
				{
					luma_range.min = MIN(luma_range.min, luma_row[li + i + k]);
					luma_range.max = MAX(luma_range.max, luma_row[li + i + k]);
					luma_mu += luma_row[li + i + k];
				}		


				cr_range.min = MIN(cr_range.min, chroma_row[i].cr);
				cr_range.max = MAX(cr_range.max, chroma_row[i].cr);
				cb_range.min = MIN(cb_range.min, chroma_row[i].cb);
				cb_range.max = MAX(cb_range.max, chroma_row[i].cb);

				cr_mu += chroma_row[i].cr;
				cb_mu += chroma_row[i].cb;
			}
		}
	}

	if(NORM_VIDEO)
	{
		luma_mu /= (FRAME_W * FRAME_H);
		cr_mu /= (FRAME_W / 2) * FRAME_H;
		cb_mu /= (FRAME_W / 2) * FRAME_H;
		float luma_spread = luma_range.max - luma_range.min;
		float cr_spread = cr_range.max - cr_range.min;
		float cb_spread = cb_range.max - cb_range.min;

		for(int j = FRAME_H; j--;)
		{
			uint32_t* row = cams[0].frame_buffer + (j * FRAME_W * 2);
			uint8_t* luma_row = state->view.luma + (j * FRAME_W);
			chroma_t* chroma_row = state->view.chroma + (j * (FRAME_W >> 1));
			
			for(int i = FRAME_W / 2; i--;)
			{
				int li = i << 1;

			luma_row[li + 0] = 255 * (luma_row[li + 0] - luma_range.min) / luma_spread;
			luma_row[li + 1] = 255 * (luma_row[li + 1] - luma_range.min) / luma_spread;

/*
			chroma_row[i].cr = 255 * (chroma_row[i].cr - cr_range.min) / cr_spread;
			chroma_row[i].cb = 255 * (chroma_row[i].cb - cb_range.min) / cb_spread;
*/

			}
		}
	}


	return 0;
}


int calibration(cam_settings_t cfg)
{
	raw_action_t action = {};
		
	poll_i2c_devs(NULL, &action);

	CAL.throttle.min = CAL.throttle.max = action.throttle;
	CAL.steering.min = CAL.steering.max = action.steering;

	for(;;)
	{
		poll_i2c_devs(NULL, &action);
		calib_t last_cal = CAL;

		if(action.steering > CAL.steering.max) CAL.steering.max = action.steering;
		if(action.throttle > CAL.throttle.max) CAL.throttle.max = action.throttle;

		if(action.steering < CAL.steering.min) CAL.steering.min = action.steering;
		if(action.throttle < CAL.throttle.min) CAL.throttle.min = action.throttle;

		if(memcmp(&last_cal, &CAL, sizeof(CAL)))
		{
			fprintf(stderr, "throttle: [%f - %f]\nsteering: [%f - %f]\n",
					CAL.throttle.min, CAL.throttle.max,
					CAL.steering.min, CAL.steering.max
			       );

			// save the calibration profile
			int fd = open("actions.cal", O_CREAT | O_WRONLY, 0666);
			write(fd, &CAL, sizeof(CAL));
			close(fd);
		}

		usleep(500000);
	}
}


int collection(cam_t* cam)
{
	int started = 0;
	dataset_header_t hdr = {};
	hdr.magic = MAGIC;
	hdr.is_raw = 1;

	// write the header first
	write(1, &hdr, sizeof(hdr));

	for(;;)
	{
		raw_state_t state = {};
		raw_action_t action = {};

		cam_request_frame(cam);

		if(poll_i2c_devs(&state, &action))
		{
			fprintf(stderr, "Error reading from i2c devices\n");
			return -1;
		}

		if(poll_vision(&state, cam))
		{
			fprintf(stderr, "Error capturing frame\n");
			return -2;
		}
/*

		int gas = action.throttle;
		if((THROTTLE_STOPPED - 3) >= gas && gas <= (THROTTLE_STOPPED + 3))
		{
			if(!started)
			{
				continue;
			}

			fprintf(stderr, "Finished\n");
			//break;
		}
		else
		{
			started = 1;
		}
*/
		raw_example_t ex = { state, action };
		if(write(1, &ex, sizeof(ex)) != sizeof(ex))
		{
			fprintf(stderr, "Error writing state-action pair\n");
			return -3;
		}
	}
}


int main(int argc, const char* argv[])
{
	int res;
	cam_settings_t cfg = {
		.width  = 160,
		.height = 120
	};

	proc_opts(argc, argv);

	fprintf(stderr, "Sensors...");

	cam_t cam[2] = {
		cam_open("/dev/video0", &cfg),
		//cam_open("/dev/video1", &cfg),
	};

	if((res = i2c_init("/dev/i2c-1")))
	{
		fprintf(stderr, "I2C init failed (%d)\n", res);
		return -1;
	}

	fprintf(stderr, "OK\n");

	switch(MODE)
	{
		case COL_MODE_ACT_CAL:
			calibration(cfg);
			break;
		default:
			res = collection(cam);
	}


	return res;
}
