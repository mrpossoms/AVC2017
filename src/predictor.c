#include <dirent.h>
#include "sys.h"
#include "structs.h"
#include "dataset_hdr.h"
#include "i2c.h"
#include "drv_pwm.h"
#include "pid.h"

int INPUT_FD = 0;

waypoint_t* WAYPOINTS;
waypoint_t* NEXT_WPT;

calib_t CAL;
uint8_t PWM_CHANNEL_MSK = 0x6; // all echo 
int FORWARD_STATE = 0;
int I2C_BUS;
int USE_DEADRECKONING = 1;

PID_t PID_THROTTLE = {
	.p = 1,
	.i = 0.25,
	.d = 2,
};

typedef struct {
	chroma_t min, max;
	int badness;
} color_range_t;

color_range_t *BAD_COLORS;
color_range_t *GOOD_COLORS;
int BAD_COUNT, GOOD_COUNT;
float TOTAL_DISTANCE;

void proc_opts(int argc, char* const *argv)
{
	int c;
	while((c = getopt(argc, argv, "fr:sm:d:")) != -1)
	switch(c)
	{	
		case 'f':
			FORWARD_STATE = 1;
			break;
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
		case 'd':
			USE_DEADRECKONING = optarg[0] == 'y' ? 1 : 0;
			break;
	
	}	
}


color_range_t* load_colors(const char* path, int* color_count)
{
	DIR* dir = opendir(path);
	color_range_t* colors = NULL;
	*color_count = 0;

	if(!dir)
	{
		b_log("opendir() failed on '%s'", path);
		return NULL;
	}

	// Get our starting position so we can return after
	// the number of color files has been determined
	long start = telldir(dir);

	// count all files except for . and ..
	struct dirent* ent;
	while((ent = readdir(dir)))
	{
		if(ent->d_name[0] == '.') continue;
		++*color_count;
	}

	// allocate array for all the colors we found
	colors = calloc(sizeof(color_range_t), *color_count );
	if(!colors)
	{
		b_log("calloc() failed for %d colors", *color_count);
		closedir(dir);
		return NULL;
	}

	// return to the beginning now load each file into our
	// array we allocated above.
	seekdir(dir, start);
	color_range_t* color = colors;
	while((ent = readdir(dir)))
	{
		if(ent->d_name[0] == '.') continue;

		char buf[256];
		snprintf(buf, sizeof(buf), "%s/%s", path, ent->d_name);
		int fd = open(buf, O_RDONLY);
	
		if(fd < 0)
		{
			b_log("open() failed on '%s'", buf);
			free(colors);
			closedir(dir);
			return NULL;
		}

		read(fd, color, sizeof(chroma_t) << 1);
		close(fd);

		// set the 'badness' of the color based on the filename
		// the more positive, the 'badder' the color. Colors with
		// high badness are avoided. Conversly, the more negative color
		// the 'gooder' it is, which leads to seeking behavior
		color->badness = atoi(ent->d_name);

		b_log("color: %s [%d-%d], [%d-%d]", 
			buf, 
			color->min.cr, color->max.cr,
			color->min.cb, color->max.cb
		);

		color++;
	}

	// cleanup
	closedir(dir);

	return colors;
}


float avoider(raw_state_t* state, float* confidence)
{
	const int CHROMA_W = FRAME_W / 2;
	int red_hist[FRAME_W / 2] = {};
	float hist_sum = 0;
	int biggest = 0;

	// Here we compute the histogram of badness values
	for(int c = CHROMA_W; c--;)
	{
		int col_sum = 0;

		// For each column, we count the 'badness' by checking for good or
		// bad pixel colors and summing up their 'badness' values
		for(int r = FRAME_H; r--;)
		{
			chroma_t cro = state->view.chroma[r * CHROMA_W + c];
			color_range_t* bad = BAD_COLORS;
			color_range_t* good = GOOD_COLORS;

			// We check the current pixel against all the bad colors
			// accumulating that bad color's 'badness' if it's a match
			for(int col = BAD_COUNT; col--;)
			{
				if(bad->min.cr <= cro.cr && cro.cr <= bad->max.cr) 
				if(bad->min.cb <= cro.cb && cro.cb <= bad->max.cb)
				{	
					col_sum += bad->badness;
					state->view.luma[(c << 1) + (r * FRAME_W)] = 16;
				}
				
				bad++;
			}

			// Same as above, but for good colors.
			for(int col = GOOD_COUNT; col--;)
			{
				if(good->min.cr <= cro.cr && cro.cr <= good->max.cr) 
				if(good->min.cb <= cro.cb && cro.cb <= good->max.cb)
				{	
					col_sum -= good->badness;
					state->view.luma[(c << 1) + (r * FRAME_W)] = 255;
				}
				
				good++;
			}
		}

		// Try to reduce noise by ignoring columns with a badness below 16
		if(col_sum > 16)
		{
			hist_sum += col_sum;
			red_hist[c] = col_sum;
		}
	}

	int best = hist_sum;
	int cont_r[2] = { CHROMA_W, CHROMA_W };

	// Here we pick the best, most contigious horizontal range of
	// the frame with the smallest sum of 'bad' colors, or the
	// largest sum of good colors if they are present.
	for(int j = CHROMA_W + 1; j--;)
	{
		int cost = 0;
		int cont_start = j;

		// From the current column, slide all the way to the left.
		for(int i = j; i--;)
		{
			// Accumulate cost for this region
			cost += red_hist[i];

			// The score for this region also factors in the width
			// so as the region grows without accumulating badness
			// it becomes more attractive. If it is found to have a better
			// score than the best, then set it as the new best.
			if((cost - (j - i)) < best)
			{
				cont_r[0] = i;
				cont_r[1] = cont_start;
				best = cost - (j - i);

				if(red_hist[i] > biggest)
				{
					biggest = red_hist[i];
				}
			}
		}
	}
	

	int cont_width = (cont_r[1] - cont_r[0]);
	int target_idx;

	// Decide which place in the region we should steer toward
	if(cont_r[0] > 0 && cont_r[1] == CHROMA_W)
	{
		target_idx = cont_r[0] + cont_width * 0.75f;
	}
	else if(cont_r[0] == 0 && cont_r[1] < CHROMA_W)
	{
		target_idx = cont_r[0] + cont_width * 0.25f;
	}
	else
	{
		target_idx = cont_r[0] + (cont_width >> 1);	
	}

	if(FORWARD_STATE)
	{
		// Draw a black line down the frame where we are steering
		for(int i = FRAME_H; i--;)
		{
			state->view.luma[i * FRAME_W + (target_idx << 1)] = 0;
		}

		// Tint the contiguous region green before forwarding the frame
		for(int j = cont_r[0]; j < cont_r[1]; ++j)
		{
			for(int i = FRAME_H; i--;)
			{
				state->view.chroma[i * CHROMA_W + j].cb -= 64;
				state->view.chroma[i * CHROMA_W + j].cr -= 64;
			}
		}
	}

	float weight = biggest / (float)FRAME_H;
	float fp = (target_idx / (float)CHROMA_W);
	static float ap;

	*confidence = MIN(weight + 0.5, 1);
	ap =  0.8 * ap + 0.2 * (1 - fp);

	return ap;

}

time_t LAST_SECOND;
raw_action_t predict(raw_state_t* state, waypoint_t goal)
{

	quat q = { 0, 0, sin(M_PI / 4), cos(M_PI / 4) };
	raw_action_t act = { 117, 117 };
	vec3 goal_vec, dist_vec = {};
	vec3 left, proj;
	float p = 0.5;

	// Visual obstacle avoidance
	float conf = 0;
	float avd_p = avoider(state, &conf);
	float inv_conf = 1 - conf;

	// Pre recorded path guidance
	if(USE_DEADRECKONING)
	{
		if(vec3_len(state->heading) == 0 && I2C_BUS > -1)
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
		p = (vec3_mul_inner(left, goal_vec) + 1) / 2;
		
		// If pointing away, steer all the way to the right or left, so
		// p will be either 1 or 0
		if(coincidence < 0)
		{
			p = roundf(p); 
		}

		p = inv_conf * p + conf * avd_p;	
	}
	else
	{
		p = avd_p;
	}


	// Lerp between right and left.
	act.steering = CAL.steering.max * (1 - p) + CAL.steering.min * p;

	// Use a pid controller to regulate the throttle to match the speed driven
	act.throttle = 117 + PID_control(&PID_THROTTLE, NEXT_WPT->velocity * inv_conf, state->vel);
	act.throttle = MAX(117, act.throttle);

	//time_t now = time(NULL);
	//if(LAST_SECOND != now)
	{
		b_log("throttle: %d", act.throttle);
	//	LAST_SECOND = now;
	}

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


waypoint_t* best_waypoint(raw_state_t* state)
{
	waypoint_t* best = NEXT_WPT;
	waypoint_t* next = NEXT_WPT;
	float lowest_cost = 1E10;

	while(next)
	{
		vec3 delta;  // difference between car pos and waypoint pos
		vec3 dir;    // unit vector direction to waypoint from car pos
		float cost;
		float co;    // coincidence with heading
		float dist;
	
		vec3_sub(delta, next->position, state->position);
		vec3_norm(dir, delta);
		co = vec3_mul_inner(dir, state->heading);
		dist = vec3_len(delta);

		cost = dist + (2 - (co + 1));
		
		if(dist > 0.5)
		{
			if(cost < lowest_cost)
			{
				best = next;
				lowest_cost = cost;	
			}
		}
		else if(next->next == NULL)
		{
			return NULL;
		}

		next = next->next; // lol
	}

	return best;
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

	proc_opts(argc, argv);

	if(i2c_init("/dev/i2c-1"))
	{
		b_log("Failed to init i2c bus");
		I2C_BUS = -1;
		//return -2;
	}
	else
	{
		pwm_set_echo(PWM_CHANNEL_MSK);
	}	

	raw_example_t ex = {};
	
	b_log("\t\033[0;32mGOOD\033[0m");
	GOOD_COLORS = load_colors("/var/predictor/color/good", &GOOD_COUNT);

	b_log("\t\033[0;31mBAD\033[0m");
	BAD_COLORS = load_colors("/var/predictor/color/bad", &BAD_COUNT);

	// If we are opting out of using deadreckoning, then figure out how
	// far we expect to travel, so we know when to terminate.
	if(!USE_DEADRECKONING)
	{
		while(NEXT_WPT)
		{
			waypoint_t* wpt = NEXT_WPT->next;
			vec3 delta;

			if(!wpt) break;

			vec3_sub(delta, wpt->position, NEXT_WPT->position);
			TOTAL_DISTANCE += vec3_len(delta);
		}
	}

	b_log("Waiting...");

	dataset_header_t hdr = {};
	read(INPUT_FD, &hdr, sizeof(hdr));
	read(INPUT_FD, &ex, sizeof(ex)); // block for the first sample
	b_log("OK");

	if(FORWARD_STATE)
	{
		write(1, &hdr, sizeof(hdr));
		write(1, &ex, sizeof(ex));
	}

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

			//if(I2C_BUS > -1)
			{
				pwm_set_action(&act);
			}

			if(USE_DEADRECKONING)
			{
				waypoint_t* next = best_waypoint(&ex.state);
				if(next != NEXT_WPT)
				{
					NEXT_WPT = next;
					b_log("next waypoint: %lx", (unsigned int)NEXT_WPT);
				}

				if(NEXT_WPT == NULL)
				{
					sig_handler(0);
				}
			}		
			else
			{
				if(ex.state.distance >= TOTAL_DISTANCE)
				{
					sig_handler(0);
				}
			}

			if(FORWARD_STATE) 
			{
				write(1, &ex, sizeof(ex));
			}
		}
		else // timeout
		{
			b_log("timeout");
			if(I2C_BUS > -1)
			{
				// stop everything 
				raw_action_t act = { 117, 117 };
				pwm_set_action(&act);
			}
		}

	}

	b_log("terminating");

	return 0;
}
