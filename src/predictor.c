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
	while((c = getopt(argc, argv, "r:sm:")) != -1)
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
			if(fd < 0)
			{
				b_log("Loading route: '%s' failed", optarg);
				exit(-1);
			}

			dataset_header_t hdr = {};
			read(fd, &hdr, sizeof(hdr));
			assert(hdr.magic == MAGIC);
			off_t all_bytes = lseek(fd, 0, SEEK_END);
			size_t count = all_bytes / sizeof(waypoint_t);

			b_log("Loading route with %d waypoints", count);

			lseek(fd, sizeof(hdr), SEEK_SET);
			WAYPOINTS = (waypoint_t*)calloc(count + 1, sizeof(waypoint_t));	
			size_t read_bytes = read(fd, WAYPOINTS, count * sizeof(waypoint_t));
			if(read_bytes != count * sizeof(waypoint_t))
			{
				b_log("route read of %d/%dB failed", read_bytes, all_bytes);
				exit(-1);
			}
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

float LAST_S=117;
raw_action_t predict(raw_state_t* state, waypoint_t goal)
{
	quat q = { 0, 0, sin(M_PI / 4), cos(M_PI / 4) };
	raw_action_t act = { 117, 117 };
	vec3 goal_vec, dist_vec = {};
	vec3 left, proj;

	if(vec3_len(state->heading) == 0)
	{
		return act;
	}

	// Compute the total delta vector between the car, and the goal
	// then normalize the delta to get the goal heading
	// rotate the heading vector about the z-axis to get the left vector
	vec3_sub(dist_vec, goal.position, state->position);
	vec3_norm(goal_vec, dist_vec);
	quat_mul_vec3(left, q, state->heading);

	// Determine if we are pointing toward the goal with the 'coincidence' value
	// Project the goal vector onto the left vector to determine steering direction
	// remap range to [0, 1] 
	float coincidence = vec3_mul_inner(state->heading, goal_vec);
	float p = (vec3_mul_inner(left, goal_vec) + 1) / 2;


	

	// If pointing away, steer all the way to the right or left, so
	// p will be either 1 or 0
	if(coincidence < 0)
	{
		p = roundf(p); 
	}

	// Lerp between right and left.
	act.steering = CAL.steering.max * (1 - p) + CAL.steering.min * p;
/*
	b_log("left (%f, %f, %f)", left[0], left[1], left[2]); 
	b_log("heading (%f, %f, %f)", state->heading[0], state->heading[1], state->heading[2]); 
	b_log("position (%f, %f, %f)", state->position[0], state->position[1], state->position[2]); 
	b_log("goal_vec (%f, %f, %f)", goal_vec[0], goal_vec[1], goal_vec[2]); 
	b_log("%f", p);	
	b_log("steering: %d", act.steering);
*/
	//if(fabs(LAST_S - act.steering) < 5) sleep(1);
	LAST_S = act.steering;
	act.throttle = 122;

	return act;
}


void sig_handler(int sig)
{
	b_log("Caught signal %d", sig);	
	raw_action_t act = { 117, 117 };
	pwm_set_echo(0x6);
	usleep(10000);
	exit(0);
}


int near_waypoint(raw_state_t* state)
{
	vec3 diff;

	vec3_sub(diff, state->position, NEXT_WPT->position);
	float len = vec3_len(diff);

	b_log("%f", len);

	return vec3_len(diff) < 1 ? 1 : 0; 
}


int main(int argc, char* const argv[])
{
	PROC_NAME = argv[0];

	signal(SIGINT, sig_handler);

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

	//pwm_reset();

	b_log("Waiting...");
	read(INPUT_FD, &ex, sizeof(ex)); // block for the first sample
	b_log("OK");

	pwm_set_echo(PWM_CHANNEL_MSK);

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
			read(INPUT_FD, &ex, sizeof(ex));

			raw_action_t act = predict(&ex.state, *NEXT_WPT); 

			pwm_set_action(&act);

			if(near_waypoint(&ex.state))
			{
				NEXT_WPT = NEXT_WPT->next;
				b_log("next waypoint: %lx", (unsigned int)NEXT_WPT);
			}

			if(NEXT_WPT == NULL)
			{
				sig_handler(0);
			}		
		}
		else // timeout
		{
			b_log("timeout");
			break;
		}

	}

	b_log("terminating");

	return 0;
}
