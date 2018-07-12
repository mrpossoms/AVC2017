//#include <dirent.h>
#include <stdio.h>
#include <stdarg.h>
#include "sys.h"
#include "structs.h"

#include "nn.h"

#define ROOT_MODEL_DIR "/var/model/"
#define MODEL_LAYERS 3
#define MODEL_INSTANCES 2

int INPUT_FD = 0;

waypoint_t* WAYPOINTS;
waypoint_t* NEXT_WPT;

int FORWARD_STATE = 0;
int USE_DEADRECKONING = 0;

typedef struct {
	mat_t X;
	nn_layer_t layers[MODEL_LAYERS];
} classifier_t;

classifier_t class_inst[MODEL_INSTANCES];

float TOTAL_DISTANCE;

static int arg_load_route(char flag, const char* path)
{
	// Load the route
	int fd = open(path, O_RDONLY);
	if (fd < 0)
	{
		b_log("Loading route: '%s' failed", optarg);
		exit(-1);
	}

	off_t all_bytes = lseek(fd, 0, SEEK_END);
	size_t count = all_bytes / sizeof(waypoint_t);

	b_log("Loading route with %d waypoints", count);

	lseek(fd, 0, SEEK_SET);
	WAYPOINTS = (waypoint_t*)calloc(count + 1, sizeof(waypoint_t));
	size_t read_bytes = read(fd, WAYPOINTS, count * sizeof(waypoint_t));
	if (read_bytes != count * sizeof(waypoint_t))
	{
		b_log("route read of %d/%dB failed", read_bytes, all_bytes);
		exit(-1);
	}
	close(fd);

	// connect waypoint references
	for (int i = 0; i < count - 1; ++i)
	{
		WAYPOINTS[i].next = WAYPOINTS + i + 1;
	}
	WAYPOINTS[count - 1].next = NULL;

	NEXT_WPT = WAYPOINTS;

	return 0;
}

#define BUCKET_SIZE 32
#define HIST_W (FRAME_W / BUCKET_SIZE)
#define HIST_MID (HIST_W >> 1)

typedef struct {
	int classifier_idx;
	int start_col, end_col;
	color_t* rgb;
	float* hist;
	raw_state_t* state;

	float confidence;
	float col_conf_best;
} class_job_t;

void* col_class_worker(void* params)
{
	class_job_t* job = (class_job_t*)params;

	// proxy variables
	color_t* rgb = job->rgb;
	float* hist = job->hist;
	raw_state_t* state = job->state;
	mat_t X = class_inst[job->classifier_idx].X;
	nn_layer_t* L = class_inst[job->classifier_idx].layers;

	const int CHROMA_W = FRAME_W / 2;

	for (int ci = job->start_col; ci < job->end_col; ++ci)
	{
		float col_sum = 0;
		int c = ci * BUCKET_SIZE;

		const int start = 70;
		const int height = 64;
		int r_stride = 1;
		int samples = 0;
		float col_conf_sum = 0;

		for (int r = 0; r < height;)
		{
			// slice out patches to use for activation
			for (int kr = 16; kr--;)
			for (int kc = 16; kc--;)
			{
				color_t color = rgb[((r + start + kr) * FRAME_W) + c + kc];
				X.data.f[(kr * 48) + kc * 3 + 0] = (color.r / 255.0f) - 0.5f;
				X.data.f[(kr * 48) + kc * 3 + 1] = (color.g / 255.0f) - 0.5f;
				X.data.f[(kr * 48) + kc * 3 + 2] = (color.b / 255.0f) - 0.5f;
			}

			mat_t y = *nn_predict(L, &X);
			col_sum += (-(y.data.f[0] + y.data.f[1]) + (y.data.f[2]));

			if (FORWARD_STATE)
			for (int kr = r_stride; kr--;)
			for (int kc = BUCKET_SIZE; kc--;)
			{
				float chroma_v  __attribute__ ((vector_size(8))) = {};
				float magenta_none __attribute__ ((vector_size(8))) = { 1, 1 };
				float orange_hay  __attribute__ ((vector_size(8))) = { -1, 1 };
				float green_asph  __attribute__ ((vector_size(8))) = { -1, -1 };

				chroma_v = y.data.f[0] * magenta_none + y.data.f[1] * orange_hay + y.data.f[2] * green_asph;
				chroma_v = (chroma_v + 1.f) / 2.f;

				state->view.chroma[(r + start + kr) * CHROMA_W + (c + kc) / 2].cr = chroma_v[0] * 255;
				state->view.chroma[(r + start + kr) * CHROMA_W + (c + kc) / 2].cb = chroma_v[1] * 255;

			}

			++samples;
			col_conf_sum += y.data.f[2];

			r += r_stride;
			r_stride *= 2;
		}

		float col_conf_avg = col_conf_sum / samples;
		if (col_conf_avg > job->col_conf_best) { job->col_conf_best = col_conf_avg; }

		hist[ci] = col_sum;
		job->confidence += col_conf_avg;
	}

	job->confidence /= (float)(job->end_col - job->start_col);

	return NULL;
}

void avoider(raw_state_t* state, float* throttle, float* steering)
{
	color_t rgb[FRAME_W * FRAME_H];
	time_t now = time(NULL);
	float hist[HIST_W] = {};
	float confidence = 0;
	float col_conf_best = 0;

	yuv422_to_rgb(state->view.luma, state->view.chroma, rgb, FRAME_W, FRAME_H);

	class_job_t job_params[MODEL_INSTANCES] = {};
	pthread_t job_threads[MODEL_INSTANCES];
	int job_col_width = HIST_W / MODEL_INSTANCES;
	for (int i = 0; i < MODEL_INSTANCES; i++)
	{
		job_params[i].classifier_idx = i;
		job_params[i].start_col = i * job_col_width;
		job_params[i].end_col = (i + 1) * job_col_width ;
		job_params[i].rgb = rgb;
		job_params[i].hist = hist;
		job_params[i].state = state;

		// col_class_worker(job_params + i);
		pthread_create(job_threads + i, NULL, col_class_worker, job_params + i);
	}

	confidence = 0;

	// wait for them to finish
	for (int i = 0; i < MODEL_INSTANCES; i++)
	{
		pthread_join(job_threads[i], NULL);

		confidence += job_params[i].confidence;
		if (col_conf_best < job_params[i].col_conf_best) { col_conf_best = job_params[i].col_conf_best; }
	}
	confidence /= MODEL_INSTANCES;

	// *confidence /= (float)HIST_W;

	float best = hist[HIST_W-1];
	int cont_r[2] = { HIST_W, HIST_W };

	// Here we pick the best, most contigious horizontal range of
	// the frame with the smallest sum of 'bad' colors, or the
	// largest sum of good colors if they are present.
	for (int j = HIST_W + 1; j--;)
	{
		float cost = 0;
		int cont_start = j;

		// From the current column, slide all the way to the left.
		for (int i = j; i--;)
		{
			// Accumulate cost for this region
			cost += hist[i];

			// The score for this region also factors in the width
			// so as the region grows without accumulating badness
			// it becomes more attractive. If it is found to have a better
			// score than the best, then set it as the new best.
			const float width_weight = 1;
			float total_cost = cost / (1 + (j - i) * width_weight);
			if (total_cost > best)
			{
				cont_r[0] = i;
				cont_r[1] = cont_start;
				best = total_cost;
			}
		}
	}

	int target_idx = (cont_r[0] + cont_r[1]) >> 1;

	// Decide which place in the region we should steer toward
	if (target_idx > HIST_MID)
	{
		target_idx = cont_r[1];// + cont_width * 0.75f;
	}
	else if (target_idx < HIST_MID)
	{
		target_idx = cont_r[0];// + cont_width * 0.25f;
	}

	float fp = (target_idx / (float)HIST_W);
	static struct {
		float steering;
		float throttle;
	} lpf;
	static time_t backup_start = 0;

	{ // Color the region and target index for debugging
		int ci;

		for (ci = cont_r[0] * BUCKET_SIZE; ci < cont_r[1] * BUCKET_SIZE; ci+=2)
		for (int i = FRAME_H; i--;)
		{
			state->view.luma[i * FRAME_W + ci] = 128;
		}

		ci = fp * FRAME_W;
		for (int i = FRAME_H; i--;)
		{
			state->view.luma[i * FRAME_W + ci] = 0;
		}
	}

	// filter the steering and throttle values.
	lpf.steering =  0.8 * lpf.steering + 0.2 * (fp);
	lpf.throttle =  0.9 * lpf.throttle + 0.1 * (confidence);

	// Let the confidence infered from classifying the video
	// frame to directly set the throttle
	if (confidence > 0.25)
	{
		lpf.throttle = confidence;
	}
	else
	{ // Or start backing up if, confidence is poor.
		backup_start = now + 1;
	}


	if (confidence <= 0.25 || col_conf_best < 0.4)
	{ // force a reverse throttle value if we are backing up
		lpf.throttle = -0.05;
	}


	if (lpf.throttle < 0.25f)
	{ // flip steering angle if reversing
		*steering = lpf.steering < 0.5f ? 1 : 0;
	}
	else
	{ // otherwise steer normally
		*steering = lpf.steering;
	}

	*throttle = lpf.throttle;
}

time_t LAST_SECOND;
raw_action_t predict(raw_state_t* state, waypoint_t* goal)
{
	raw_action_t act = { 117, 117 };

	// Visual obstacle avoidance
	float steer = 0.5, throttle = 0.5;
	avoider(state, &throttle, &steer);

	// Lerp between right and left.
	act.steering = 255 * steer; //CAL.steering.max * (1 - p) + CAL.steering.min * p;

	// Use a pid controller to regulate the throttle to match the speed driven
	act.throttle = 100 + throttle * 155;

	return act;
}


void sig_handler(int sig)
{
	b_log("Caught signal %d", sig);
	exit(0);
}


waypoint_t* best_waypoint(raw_state_t* state)
{
	waypoint_t* next = NEXT_WPT;
	waypoint_t* best = NEXT_WPT;
	float dist_sum = 0;

	while(next)
	{
		vec3 delta;  // difference between car pos and waypoint pos

		if (USE_DEADRECKONING)
		{
			vec3 dir;    // unit vector direction to waypoint from car pos
			float cost;
			float co;    // coincidence with heading
			float dist;
			float lowest_cost = 1E10;

			vec3_sub(delta, next->position, state->position);
			vec3_norm(dir, delta);
			co = vec3_mul_inner(dir, state->heading);
			dist = vec3_len(delta);

			cost = dist + (2 - (co + 1));

			if (dist > 0.5)
			{
				if (cost < lowest_cost)
				{
					best = next;
					lowest_cost = cost;
				}
			}
			else if (next->next == NULL)
			{
				return NULL;
			}

		}
		else
		{
			if (!next->next) return NULL;

			vec3_sub(delta, next->position, next->next->position);
			dist_sum += vec3_len(delta);

			if (dist_sum > state->distance)
			{
				return next;
			}
		}

		next = next->next; // lol
	}

	return best;
}


int main(int argc, char* const argv[])
{
	PROC_NAME = argv[0];

	signal(SIGINT, sig_handler);

	// load and instantiate multiple instances of the model and
	// feature vectors for paralellized classification
	for (int i = MODEL_INSTANCES; i--;)
	{
		mat_t x = { .dims = { 1, 768 } };
		nn_layer_t layers[3] = {
			{
				.w = nn_mat_load(ROOT_MODEL_DIR "dense.kernel"),
				.b = nn_mat_load(ROOT_MODEL_DIR "dense.bias"),
				.activation = nn_act_relu
			},
			{
				.w = nn_mat_load(ROOT_MODEL_DIR "dense_1.kernel"),
				.b = nn_mat_load(ROOT_MODEL_DIR "dense_1.bias"),
				.activation = nn_act_softmax
			},
			{}
		};
		memcpy(class_inst[i].layers, layers, sizeof(layers));
		class_inst[i].X = x;

		nn_mat_init(&class_inst[i].X);
		nn_init(class_inst[i].layers, &class_inst[i].X);
	}

	// Define and process command line, args
	cli_cmd_t cmds[] = {
		{ 'f',
			.desc = "Forward full system state over stdout",
			.set = &FORWARD_STATE,
		},
		{ 'r',
			.desc = "Path for route file to load",
			.set = arg_load_route,
			.type = ARG_TYP_CALLBACK
		},
		{ 'd',
			.desc = "Enable deadreckoning",
			.set = &USE_DEADRECKONING,
		},
		{}
	};
	cli("Collects data from sensors, compiles them into system\n"
	    "state packets. Then forwards them over stdout", cmds, argc, argv);

	b_log("Waiting...");

	while(1)
	{
		message_t msg = {};

		if (!read_pipeline_payload(&msg, PAYLOAD_STATE))
		{
			raw_state_t* state = &msg.payload.state;
			raw_action_t act = predict(state, NEXT_WPT);

			msg.header.type = PAYLOAD_ACTION;
			msg.payload.pair.action = act;

			if (USE_DEADRECKONING)
			{
				b_log("Reckoning...");
				waypoint_t* next = best_waypoint(state);
				if (next != NEXT_WPT)
				{
					NEXT_WPT = next;
					b_log("next waypoint: %lx", (unsigned int)NEXT_WPT);
				}

				if (NEXT_WPT == NULL)
				{
					b_bad("No waypoints loaded");
					exit(-2);
				}
			}

			if (FORWARD_STATE)
			{
				msg.header.type = PAYLOAD_PAIR;
			}

			if (write_pipeline_payload(&msg))
			{
				b_bad("Failed to write payload");
				return -1;
			}
		}
		else
		{
			b_bad("read error");

			return -1;
		}
	}

	b_bad("terminating");

	return 0;
}
