//#include <dirent.h>
#include <stdio.h>
#include <stdarg.h>
#include <semaphore.h>
#include "sys.h"
#include "structs.h"

#include "nn.h"
#include "cfg.h"

#define USE_CNN
#define ROOT_MODEL_DIR "/etc/bot/predictor/model/"
#define MODEL_LAYERS 4
#define MODEL_INSTANCES 2

#define BUCKETS 10
#define BUCKET_SIZE (FRAME_W / BUCKETS)
#define HIST_W (FRAME_W / BUCKET_SIZE)
#define HIST_MID (HIST_W >> 1)

#define PATCH_SIZE 12

#define MAX_POOL_HALF {          \
    .type = POOLING_MAX,         \
    .op = {                      \
       .padding = PADDING_VALID, \
       .stride = { 2, 2 },       \
       .kernel = { 2, 2 },       \
    }                            \
}\

int INPUT_FD = 0;

struct {
	int forward_state;
	int debug_colors;
	int use_deadreckoning;
	int print_refresh_rate;
} cli_cfg = { };

typedef struct {
	mat_t X;
	nn_layer_t layers[MODEL_LAYERS];
} classifier_t;

classifier_t class_inst[MODEL_INSTANCES];

float TOTAL_DISTANCE;

typedef struct {
	pthread_mutex_t start_gate;
	sem_t* sem;
	int classifier_idx;
	int start_col, end_col;
	color_t* rgb;
	float* hist;
	raw_state_t* state;

	float confidence;
	float col_conf_best;
} class_job_t;


static int log_verbosity_cb(char flag, const char* v)
{
	LOG_VERBOSITY++;
	return 0;
}


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
	int start_row = cfg_int("classifier/start-row", 30);
	const int height = cfg_int("classifier/rows", 32);
	const int stride_coeff = cfg_int("classifier/stride-coeff", 1);
	const int stride_start = cfg_int("classifier/stride-start", 8);

start:
	pthread_mutex_lock(&job->start_gate);

	job->confidence = 0;
	job->col_conf_best = 0;

	for (int ci = job->start_col; ci < job->end_col; ++ci)
	{
		float col_sum = 0;
		int c = ci * BUCKET_SIZE;

		const int start = start_row;
		int r_stride = stride_start;
		int samples = 0;
		float col_conf_sum = 0;

		for (int r = 0; r < height;)
		{
			rectangle_t patch = { c, r + start, PATCH_SIZE, PATCH_SIZE };
			image_patch_f(X.data.f, rgb, patch);

			// pass the patch through the network, make predictions
			mat_t y = *nn_predict(L, &X);
			col_sum += (-(y.data.f[0] + y.data.f[1]) + (y.data.f[2]));

			//  if specified, colorize classified patches from frame for debugging
			if (cli_cfg.debug_colors)
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
			col_conf_sum += y.data.f[2] > y.data.f[0] && y.data.f[2] > y.data.f[1] ? 1 : 0;

			r += r_stride;
			r_stride *= stride_coeff;
		}

		float col_conf_avg = col_conf_sum / samples;
		if (col_conf_avg > job->col_conf_best) { job->col_conf_best = col_conf_avg; }

		hist[ci] = col_sum;
		job->confidence += col_conf_avg;
	}

	job->confidence /= (float)(job->end_col - job->start_col);
	sem_post(job->sem);
goto start;

	return NULL;
}

void avoider(raw_state_t* state, float* throttle, float* steering)
{
	color_t rgb[FRAME_W * FRAME_H];
	float hist[HIST_W] = {};
	float confidence = 0;
	float col_conf_best = 0;

	static struct {
		float steering;
		float throttle;
	} lpf;

	yuv422_to_rgb(state->view.luma, state->view.chroma, rgb, FRAME_W, FRAME_H);

	static int setup;
	static sem_t* classifier_synch_sem;
	static class_job_t job_params[MODEL_INSTANCES];
	static pthread_t job_threads[MODEL_INSTANCES];
	int job_col_width = HIST_W / MODEL_INSTANCES;

	if (!setup)
	{
		sem_unlink("avc_vis_sem");
		classifier_synch_sem = sem_open("avc_vis_sem", O_CREAT, 0666, 0);
		if(classifier_synch_sem == SEM_FAILED)
		{
			b_bad("sem_open() - failed for visual multiprocessing");
			exit(-3);
		}

		lpf.steering = 0.5;
		lpf.throttle = 0.5;

		for (int i = 0; i < MODEL_INSTANCES; i++)
		{
			job_params[i].sem = classifier_synch_sem;
			job_params[i].classifier_idx = i;
			job_params[i].start_col = i * job_col_width;
			job_params[i].end_col = (i + 1) * job_col_width;
			job_params[i].rgb = rgb;
			job_params[i].hist = hist;
			job_params[i].state = state;

			if (i == MODEL_INSTANCES - 1)
			{
				job_params[i].end_col = HIST_W;
			}

			pthread_mutex_init(&job_params[i].start_gate, NULL);
			pthread_mutex_lock(&job_params[i].start_gate);
			pthread_create(job_threads + i, NULL, col_class_worker, job_params + i);
		}
		setup = 1;
	}

	for (int i = 0; i < MODEL_INSTANCES; i++)
	{
		pthread_mutex_unlock(&job_params[i].start_gate);
	}

	confidence = 0;

	// wait for them to finish
	for (int i = 0; i < MODEL_INSTANCES; i++)
	{
		sem_wait(classifier_synch_sem);
	}

	for (int i = 0; i < MODEL_INSTANCES; i++)
	{
		confidence += job_params[i].confidence;
		if (col_conf_best < job_params[i].col_conf_best) { col_conf_best = job_params[i].col_conf_best; }
	}

	confidence /= MODEL_INSTANCES;

	// *confidence /= (float)HIST_W;

	float best_score = -100;//hist[HIST_W-1];
	int cont_r[2] = { 0, HIST_W };

	// Here we pick the best, most contiguous horizontal range of
	// the frame with the smallest sum of 'bad' colors, or the
	// largest sum of good colors if they are present.
	for (int j = HIST_W + 1; j--;)
	{
		float score = 0;
		int cont_start = j;

		// From the current column, slide all the way to the left.
		for (int i = j; i--;)
		{
			// Accumulate score for this region
			score += hist[i];

			// The score for this region also factors in the width
			// so as the region grows without accumulating badness
			// it becomes more attractive. If it is found to have a better
			// score than the best, then set it as the new best.
			const float width_weight = 1;
			float total_score = score / (1 + (j - i) * width_weight);
			if (total_score > best_score)
			{
				cont_r[0] = i;
				cont_r[1] = cont_start;
				best_score = total_score;
			}
		}
	}

	int target_idx = (cont_r[0] + cont_r[1]) >> 1;

	// Decide which place in the region we should steer toward
	if (cont_r[0] > HIST_MID && cont_r[1] > HIST_MID)
	{
		target_idx = cont_r[1];// + cont_width * 0.75f;
	}
	else if (cont_r[0] < HIST_MID && cont_r[1] < HIST_MID)
	{
		target_idx = cont_r[0];// + cont_width * 0.25f;
	}

	float fp = (target_idx / (float)HIST_W);

	if (cli_cfg.debug_colors)
	{ // Color the region and target index for debugging
		int ci;

		for (ci = cont_r[0] * BUCKET_SIZE; ci < cont_r[1] * BUCKET_SIZE; ci+=2)
		for (int i = FRAME_H; i--;)
		{
			state->view.luma[i * FRAME_W + ci] = 128;
		}

		ci = fp * (FRAME_W - 1);
		for (int i = FRAME_H; i--;)
		{
			state->view.luma[i * FRAME_W + ci] = 0;
		}
	}

	// filter the steering and throttle values.
	const float lpf_w = 0.5f;
	lpf.steering =  (1.f - lpf_w) * lpf.steering + lpf_w * (fp);
	lpf.throttle =  (1.f - lpf_w) * lpf.throttle + lpf_w * (confidence);

	// Let the confidence infered from classifying the video
	// frame to directly set the throttle
	if (confidence > 0.25)
	{
		lpf.throttle = confidence;
	}

	if (confidence <= 0.25 || col_conf_best < 0.4)
	{ // force a reverse throttle value if we are backing up
		lpf.throttle = -0.05;
	}


	// TODO
	if (lpf.throttle < 0.25f)
	{ // flip steering angle if reversing
		*steering = lpf.steering < 0.5f ? 1 : 0;
	}
	else
	{ // otherwise steer normally
		const float amp = cfg_float("steering_amp", 1.0f);
		*steering = ((lpf.steering - 0.5f) * amp) + 0.5f;
		*steering = CLAMP(*steering, 0, 1);

		LOG_LVL(2) b_log("s: %f, t: %f", lpf.steering, lpf.throttle);
	}

	*throttle = lpf.throttle;
}

time_t LAST_SECOND;
raw_action_t predict(raw_state_t* state)
{
	raw_action_t act = { 117, 117 };

	// Visual obstacle avoidance
	float steer = 0.5, throttle = 0.5;
	avoider(state, &throttle, &steer);

	// Lerp between right and left.
	act.steering = steer * 254;//CAL.steering.max * (1 - steer) + CAL.steering.min * steer;

	// Use a pid controller to regulate the throttle to match the speed driven
	act.throttle = 100 + throttle * 155;

	return act;
}


void sig_handler(int sig)
{
	LOG_LVL(1) b_log("Caught signal %d", sig);
	sem_unlink("avc_vis_sem");
	exit(0);
}


int main(int argc, char* const argv[])
{
	PROC_NAME = argv[0];

	signal(SIGINT, sig_handler);

	cfg_base("/etc/bot/predictor/");

#ifdef __linux__
	struct sched_param sch_par = {
		.sched_priority = 50,
	};

	if (sched_setscheduler(0, SCHED_RR, &sch_par) != 0)
	{
		b_bad("RT-scheduling not set");
	}
#endif

	// load and instantiate multiple instances of the model and
	// feature vectors for parallelized classification
	mat_t x = {
#ifdef USE_CNN
		.dims = { PATCH_SIZE, PATCH_SIZE, 3 },
#else
		.dims = { 1, PATCH_SIZE * PATCH_SIZE * 3 },
#endif
		.row_major = 1,
		.is_activation_map = 1,
	};
	nn_layer_t template[] = {
#ifdef USE_CNN
		// cnn2
		{
			.w = nn_mat_load_row_order(ROOT_MODEL_DIR "c0.w", 0),
			.b = nn_mat_load_row_order(ROOT_MODEL_DIR "c0.b", 1),
			.activation = nn_act_relu,
			.filter = {
				.kernel = { 3, 3 },
				.stride = { 1, 1 },
				.padding = PADDING_VALID,

			},
			.pool = MAX_POOL_HALF,
		},
		{
			.w = nn_mat_load_row_order(ROOT_MODEL_DIR "c1.w", 0),
			.b = nn_mat_load_row_order(ROOT_MODEL_DIR "c1.b", 1),
			.activation = nn_act_linear,
			//.activation = nn_act_softmax,
			.filter = {
				.kernel = { 3, 3 },
				.stride = { 1, 1 },
				.padding = PADDING_VALID,

			},
		},
		{
			.w = nn_mat_load_row_order(ROOT_MODEL_DIR "c2.w", 0),
			.b = nn_mat_load_row_order(ROOT_MODEL_DIR "c2.b", 1),
			//.activation = nn_act_linear,
			.activation = nn_act_softmax,
			.filter = {
				.kernel = { 1, 1 },
				.stride = { 1, 1 },
				.padding = PADDING_VALID,
			},
		},
#else
		// fc2
		{
			.w = nn_mat_load_row_order(ROOT_MODEL_DIR "fc0.w", 0),
			.b = nn_mat_load_row_order(ROOT_MODEL_DIR "fc0.b", 1),
			.activation = nn_act_relu
		},
		{
			.w = nn_mat_load_row_order(ROOT_MODEL_DIR "fc1.w", 0),
			.b = nn_mat_load_row_order(ROOT_MODEL_DIR "fc1.b", 1),
			.activation = nn_act_softmax
		},
#endif

		// // cnn3
		// {
		// 	.w = nn_mat_load_row_order(ROOT_MODEL_DIR "c0.w", 0),
		// 	.b = nn_mat_load_row_order(ROOT_MODEL_DIR "c0.b", 1),
		// 	.activation = nn_act_relu,
		// 	.filter = {
		// 		.kernel = { 5, 5 },
		// 		.stride = { 1, 1 },
		// 		.padding = PADDING_VALID,

		// 	},
		// 	.pool = MAX_POOL_HALF,
		// },
		// {
		// 	.w = nn_mat_load_row_order(ROOT_MODEL_DIR "c1.w", 0),
		// 	.b = nn_mat_load_row_order(ROOT_MODEL_DIR "c1.b", 1),
		// 	// .activation = nn_act_linear,
		// 	.activation = nn_act_relu,
		// 	.filter = {
		// 		.kernel = { 5, 5 },
		// 		.stride = { 1, 1 },
		// 		.padding = PADDING_VALID,

		// 	},
		// 	// .pool = MAX_POOL_HALF,
		// },
		// {
		// 	.w = nn_mat_load_row_order(ROOT_MODEL_DIR "c2.w", 0),
		// 	.b = nn_mat_load_row_order(ROOT_MODEL_DIR "c2.b", 1),
		// 	// .activation = nn_act_linear,
		// 	.activation = nn_act_softmax,
		// 	.filter = {
		// 		.kernel = { 2, 2 },
		// 		.stride = { 1, 1 },
		// 		.padding = PADDING_VALID,

		// 	},
		// },
		{}
	};

	if (nn_init(template, &x) != 0)
	{
		b_bad("Error initializing NN");
		return -1;
	}

	for (int i = MODEL_INSTANCES; i--;)
	{
		mat_t x = {
#ifdef USE_CNN
			.dims = { PATCH_SIZE, PATCH_SIZE, 3 },
#else
			.dims = { 1, PATCH_SIZE * PATCH_SIZE * 3 },
#endif
			.row_major = 1,
			.is_activation_map = 1,
		};
		class_inst[i].X = x;
		nn_mat_init(&class_inst[i].X);
		nn_clone(class_inst[i].layers, template, &class_inst[i].X);
	}

	// Define and process command line, args
	cli_cmd_t cmds[] = {
		{ 'f',
			.desc = "Forward full system state over stdout",
			.set  = &cli_cfg.forward_state,
		},
		{ 'c',
			.desc = "Show debug colors",
			.set  = &cli_cfg.debug_colors,
		},
		{ 'd',
			.desc = "Enable deadreckoning",
			.set  = &cli_cfg.use_deadreckoning,
		},
		{ 'r',
			.desc = "Show refresh rate",
			.set  = &cli_cfg.print_refresh_rate,
		},
		CLI_CMD_LOG_VERBOSITY,
		{}
	};
	cli("Collects data from sensors, compiles them into system\n"
	    "state packets. Then forwards them over stdout", cmds, argc, argv);

	LOG_LVL(1) b_log("Waiting...");

	// profiling vars
	unsigned int cycles = 0;
	time_t start = 0;

	while(1)
	{
		message_t msg = {};

		if (cli_cfg.print_refresh_rate)
		{
			time_t now = time(NULL);
			if (start != now)
			{
				b_log("%dhz", cycles);
				start = now;
				cycles = 0;
			}

			cycles++;
		}

		if (!read_pipeline_payload(&msg, PAYLOAD_STATE))
		{
			raw_state_t* state = &msg.payload.state;
			raw_action_t act = predict(state);

			if (cli_cfg.use_deadreckoning)
			{
				LOG_LVL(1) b_log("Reckoning...");
			}

			if (cli_cfg.forward_state)
			{
				msg.header.type = PAYLOAD_PAIR;
				msg.payload.pair.action = act;
				msg.payload.pair.state = *state;
			}
			else
			{
				msg.header.type = PAYLOAD_ACTION;
				msg.payload.action = act;
			}

			LOG_LVL(3) b_log("s:%d t:%d", act.steering, act.throttle);

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
