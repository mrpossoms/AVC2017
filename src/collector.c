#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "i2c.h"

#define THROTTLE_STOPPED 0

#define PWM_LOGGER_ADDR 0x69

typedef union {
	struct {
		uint8_t r, g, b;
	};
	uint8_t v[3];
} color_t;

typedef struct {
	uint8_t throttle, steering;
} raw_action_t;

typedef struct {
	int16_t rot_rate[3];
	int16_t acc[3];
	int8_t vel;
	color_t view[21][21];
} raw_state_t;

typedef struct {
	raw_state_t state;
	raw_action_t action;
} example_t;

int I2C_BUS;

int poll_i2c_devs(raw_state_t* state, raw_action_t* action)
{
	// TODO get sensor readings
	
	// Get throttle and steering state
	if(i2c_read(I2C_BUS, PWM_LOGGER_ADDR, 0x1, (void*)action, sizeof(raw_action_t)))
	{
		return 1;
	}	

	return 0;
}


int poll_vision(raw_state_t* state)
{
	// TODO
	return 0;
}


int main(int argc, const char* argv[])
{
	int started = 0;

	for(;;)
	{
		raw_state_t state = {};
		raw_action_t action = {};

		if(poll_i2c_devs(&state, &action))
		{
			fprintf(stderr, "Error reading from i2c devices\n");
			return -1;
		}

		if(poll_vision(&state))
		{
			fprintf(stderr, "Error capturing frame\n");
			return -2;
		}

		if(action.throttle == THROTTLE_STOPPED)
		{
			if(!started)
			{
				continue;
			}

			fprintf(stderr, "Finished\n");
			break;
		}

		example_t ex = { state, action };
		if(write(1, &ex, sizeof(ex)) != sizeof(ex))
		{
			fprintf(stderr, "Error writing state-action pair\n");
			return -3;
		}
	}

	return 0;
}
