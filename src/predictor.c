#include "sys.h"
#include "structs.h"
#include "dataset_hdr.h"
#include "i2c.h"
#include "drv_pwm.h"

int INPUT_FD = 0;

waypoint_t* WAYPOINTS;
waypoint_t* NEXT_WPT;

calib_t CAL;
uint8_t PWM_CHANNEL_MSK = 0x6; // all echo 

void proc_opts(int argc, char* const *argv)
{
	int c;
	while((c = getopt(argc, argv, "rsm:")) != -1)
	switch(c)
	{	
		case 'm':
			// Explicit PWM channel masking
			PWM_CHANNEL_MSK = atoi(optarg);
			b_log("channel mask %x", PWM_CHANNEL_MSK);
			break;
		case 'r':
		{
			// Load the route
			int fd = open(optarg, O_RDONLY);
			if(fd < 0) exit(-1);

			dataset_header_t hdr = {};
			read(fd, &hdr, sizeof(hdr));
			assert(hdr.magic == MAGIC);
			off_t len = lseek(fd, SEEK_END, 0);
			size_t count = len / sizeof(waypoint_t);
			WAYPOINTS = (waypoint_t*)calloc(count + 1, sizeof(waypoint_t));	
			read(fd, WAYPOINTS, len);
			close(fd);

			// connect waypoint references
			for(int i = 0; i < count - 1; ++i)
			{
				WAYPOINTS[i].next = WAYPOINTS + i + 1;
			}
			WAYPOINTS[count - 1].next = NULL;

			NEXT_WPT = WAYPOINTS;
		}
			break;
		case 's':
		{
			b_log("Pointing to (0, 1E6, 0)");

			static waypoint_t up = {
				.position = { 0, 1E6, 0 },
				.heading  = { 0, 1, 0 },
				.velocity = 0.25,
			};

			WAYPOINTS = &up;
			NEXT_WPT = WAYPOINTS;
		}
			break;
	}	
}


raw_action_t predict(raw_state_t* state, waypoint_t goal)
{
	quat q = { 0, 0, sin(M_PI / 4), cos(M_PI / 4) };
	raw_action_t act = {};
	vec3 goal_vec, dist_vec = {};
	vec3 left, proj;

	vec3_sub(dist_vec, goal.position, state->position);
	vec3_norm(goal_vec, dist_vec);
	quat_mul_vec3(left, q, state->heading);
	float p = vec3_mul_inner(left, goal_vec);
	
	//b_log("left (%f, %f, %f)", left[0], left[1], left[2]); 
	//b_log("heading (%f, %f, %f)", state->heading[0], state->heading[1], state->heading[2]); 
	//b_log("%f", p);	

	float mu = ((CAL.steering.max - CAL.steering.min) / 2);

	if(p < 0)
	{
		act.steering = mu * (1 - p) + CAL.steering.min;
	}
	else
	{
		act.steering = mu * (1 - p) + CAL.steering.max;
	}

	b_log("steering: %d", act.steering);

	return act;
}


void sig_handler(int sig)
{
	b_log("Caught signal %d", sig);	
}


int main(int argc, char* const argv[])
{
	PROC_NAME = argv[0];

	if(calib_load(ACTION_CAL_PATH, &CAL))
	{
		b_log("Failed to load '%s'", ACTION_CAL_PATH);
		return -1;
	}

	if(i2c_init("/dev/i2c-1"))
	{
		b_log("Failed to init i2c bus");
		return -2;
	}


	proc_opts(argc, argv);
		
	raw_example_t ex = {};

	pwm_reset();
	sleep(1);
	pwm_set_echo(PWM_CHANNEL_MSK);

	b_log("Waiting...");
	read(INPUT_FD, &ex, sizeof(ex)); // block for the first sample
	b_log("OK");

	while(1)
	{
		int ret = 0;
		fd_set fds;
		struct timeval tv = { .tv_usec = 1000 * 333 };

		FD_ZERO(&fds);
		FD_SET(INPUT_FD, &fds);

		ret = select(1, &fds, NULL, NULL, &tv);

		if(ret == -1) // error
		{
			// TODO
			b_log("Error");
		}
		else if(ret) // stuff to read
		{
			raw_action_t act = predict(&ex.state, *NEXT_WPT); 

			pwm_set_action(&act);
		}
		else // timeout
		{
			b_log("timeout");
			break;
		}

		read(INPUT_FD, &ex, sizeof(ex));
	}

	b_log("terminating");

	return 0;
}
