#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "structs.h"
#include "i2c.h"
#include "cam.h"

#define THROTTLE_STOPPED 0
int I2C_BUS;

int poll_i2c_devs(raw_state_t* state, raw_action_t* action)
{
	// Get throttle and steering state
	if(i2c_read(I2C_BUS_FD, PWM_LOGGER_ADDR, 7, (void*)action, sizeof(raw_action_t)))
	{
		return 1;
	}

	uint8_t mode = 0;
	int res = bno055_get_operation_mode(&mode);

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
	for(unsigned int i = cams[0].buffer_info.length / sizeof(uint32_t); i--;)
	{
		state->view[i >> 1].y  = fb_pixel_pair[i] & 0xFF;
		state->view[i >> 1].cb = (fb_pixel_pair[i] >> 24) & 0xFF;
		state->view[i >> 1].cr = (fb_pixel_pair[i] >> 8) & 0xFF;
	}

	return 0;
}


int main(int argc, const char* argv[])
{
	int started = 0;
	cam_settings_t cfg = {
		.width = 160,
		.height = 120
	};

	cam_t cam[2] = {
		cam_open("/dev/video0", &cfg),
		cam_open("/dev/video1", &cfg),
	};


	if(i2c_init("/dev/i2c-1"))
	{
		return -1;
	}

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

		int img_fd = open("frame.data", O_RDWR | O_CREAT | O_APPEND, 0666);
		write(img_fd, state.view, sizeof(state.view));
		close(img_fd);
		write(1, ".", 1);

		printf("a: %d %d %d\n", state.acc[0], state.acc[1], state.acc[2]);
		//printf("g: %d %d %d\n", state.rot_rate[0], state.rot_rate[1], state.rot_rate[2]);
		fflush(stdout);


		if(action.throttle == THROTTLE_STOPPED)
		{
			if(!started)
			{
				continue;
			}

			fprintf(stderr, "Finished\n");
			break;
		}

				// raw_example_t ex = { state, action };
		// if(write(1, &ex, sizeof(ex)) != sizeof(ex))
		// {
		// 	fprintf(stderr, "Error writing state-action pair\n");
		// 	return -3;
		// }
	}

	return 0;
}
