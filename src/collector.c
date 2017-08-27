#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "structs.h"
#include "i2c.h"
#include "cam.h"

#define THROTTLE_STOPPED 117
int I2C_BUS;

int poll_i2c_devs(raw_state_t* state, raw_action_t* action)
{
	// Get throttle and steering state
	if(i2c_read(I2C_BUS_FD, PWM_LOGGER_ADDR, 7, (void*)action, sizeof(raw_action_t)))
	{
		return 1;
	}

	uint16_t odo = 0;
	int res = i2c_read(I2C_BUS_FD, PWM_LOGGER_ADDR, 0x0C, (void*)&odo, sizeof(odo));

	fprintf(stderr, "odo: %d\n", odo);

	uint8_t mode = 0;
	res = bno055_get_operation_mode(&mode);

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

	// Downsample the intensity resolution to match that of
	// the chroma
	uint32_t* fb_pixel_pair = cams[0].frame_buffer;
	for(int i = FRAME_W * FRAME_H; i--;)
	{
		state->view[i].y  = fb_pixel_pair[i << 2] & 0xFF;
		state->view[i].cb = (fb_pixel_pair[i << 2] >> 24) & 0xFF;
		state->view[i].cr = (fb_pixel_pair[i << 2] >> 8) & 0xFF;
	}

	return 0;
}


int main(int argc, const char* argv[])
{
	int res, started = 0;
	cam_settings_t cfg = {
		.width  = 160,
		.height = 120
	};

	fprintf(stderr, "Sensors...");

	cam_t cam[2] = {
		cam_open("/dev/video0", &cfg),
		cam_open("/dev/video1", &cfg),
	};

	if((res = i2c_init("/dev/i2c-1")))
	{
		fprintf(stderr, "I2C init failed (%d)\n", res);
		return -1;
	}

	fprintf(stderr, "OK\n");
	fprintf(stderr, "raw_state_t: %uB, raw_action_t: %uB\n", sizeof(raw_state_t), sizeof(raw_action_t));

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

		int gas = action.throttle;
		if((THROTTLE_STOPPED - 3) >= gas && gas <= (THROTTLE_STOPPED + 3))
		{
			if(!started)
			{
				continue;
			}

			fprintf(stderr, "Finished\n");
			break;
		}
		else
		{
			started = 1;
		}

		raw_example_t ex = { state, action };
		if(write(1, &ex, sizeof(ex)) != sizeof(ex))
		{
			fprintf(stderr, "Error writing state-action pair\n");
			return -3;
		}
	}

	return 0;
}
