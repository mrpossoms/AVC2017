#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "i2c.h"
#include "cam.h"

#define THROTTLE_STOPPED 0
#define PWM_LOGGER_ADDR 0x69
int I2C_BUS;

int poll_i2c_devs(raw_state_t* state, raw_action_t* action)
{
	// TODO get sensor readings
	return 0;

	// Get throttle and steering state
	if(i2c_read(I2C_BUS, PWM_LOGGER_ADDR, 0x1, (void*)action, sizeof(raw_action_t)))
	{
		return 1;
	}

	return 0;
}


int poll_vision(raw_state_t* state, cam_t* cams)
{
	// TODO

	cam_wait_frame(cams);

	// Downsample the intensity resolution to match that of
	// the chroma
	uint32_t* fb_pixel_pair = cams[0].frame_buffer;
	for(unsigned int i = cams[0].buffer_info.length / sizeof(uint32_t); i--;)
	{
		state->view[i].y = fb_pixel_pair[i] >> 24;
		state->view[i].cb = (fb_pixel_pair[i] >> 16) & 0xFF;
		state->view[i].cr = fb_pixel_pair[i] & 0xFF;
	}

	return 0;
}


int main(int argc, const char* argv[])
{
	int started = 0;
	cam_settings_t cfg = {
		.width = FRAME_W,
		.height = FRAME_H
	};

	cam_t cam[2] = {
		cam_open("/dev/video0", &cfg),
		cam_open("/dev/video1", &cfg),
	};

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

		if(action.throttle == THROTTLE_STOPPED)
		{
			if(!started)
			{
				continue;
			}

			fprintf(stderr, "Finished\n");
			break;
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
