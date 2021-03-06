#define _GNU_SOURCE

#include "sys.h"
#include "structs.h"
#include "i2c.h"
#include "cam.h"
#include "linmath.h"
#include "deadreckon.h"
#include "cfg.h"


typedef enum {
	COL_MODE_NORMAL = 0,
	COL_MODE_ACT_CAL,
	COL_MODE_ROUTE,
} col_mode_t;

struct {
	int wait_for_movement;
	int read_action;
	int frame_rate;
	int gen_random;
} cli_cfg = {
	.wait_for_movement = 1,
	.read_action = 0,
	.frame_rate = 30,
	.gen_random = 0,
};

struct {
	float meters_to_travel;
} cfg;

int I2C_BUS;
int* READ_ACTION = &cli_cfg.read_action;
calib_t CAL;
col_mode_t MODE;
float VEL;
pthread_mutex_t STATE_LOCK;


static int arg_calibration_mode(char flag, const char* v)
{
	MODE = COL_MODE_ACT_CAL;
	b_log("Calibrating action vector");
	return 0;
}


static int arg_immediate_start(char flag, const char* v)
{
	cli_cfg.wait_for_movement = 0;
	b_log("Starting immediately");
	return 0;
}

static int arg_enable_action_polling(char flag, const char* v)
{
	cli_cfg.read_action = 1;
	return 0;
}

/**
 * @brief Sets program state according to specified command line flags
 * @param argc - number of arguments
 * @param argv - array of character pointers to arguments.
 */
void proc_opts(int argc, char* const argv[])
{
	// define and process cli args
	cli_cmd_t cmds[] = {
		{ 'c',
			.desc = "Calibrate action vector (steering and throttle)",
			.set = arg_calibration_mode,
			.type = ARG_TYP_CALLBACK
		},
		{ 'i',
			.desc = "Start immediately, don't wait for movement",
			.set = arg_immediate_start,
			.type = ARG_TYP_CALLBACK
		},
		{ 'a',
			.desc = "enable polling of PWM action values",
			.set = arg_enable_action_polling,
			.type = ARG_TYP_CALLBACK
		},
		{ 'f',
			.desc = "set framerate in frames/second",
			.set = &cli_cfg.frame_rate,
			.type = ARG_TYP_INT,
			.opts = { .has_value = 1 },
		},
		{ 'r',
			.desc = "generate random data rather than collecting.",
			.set = &cli_cfg.gen_random,
			.type = ARG_TYP_FLAG,
		},
		CLI_CMD_LOG_VERBOSITY,
		{} // terminator
	};

	cli("Collects data from sensors, compiles them into system state packets. Then forwards them over stdout",
	cmds, argc, argv);
}

/**
 * @brief Loads configuration file values
 */
void load_cfg()
{
	cfg.meters_to_travel = cfg_float("meters-to-travel", 0.f); // zero means never stop
}

/**
 * @brief Waits for the camera to finish capturing a frame.
 * @param state - Pointer to the raw state vector of the platform
 * @param cams - Array of cameras to poll.
 * @return 0 on success
 */
int poll_vision(raw_state_t* state, cam_t* cams)
{
	cam_wait_frame(cams);

	// Downsample the intensity resolution to match that of
	// the chroma
	for (int j = FRAME_H; j--;)
	{
		int bi = cams[0].buffer_info.index;
		uint32_t* row = cams[0].frame_buffers[bi] + (j * (FRAME_W << 1));
		uint8_t* luma_row = state->view.luma + (j * FRAME_W);
		chroma_t* chroma_row = state->view.chroma + (j * (FRAME_W >> 1));

		for (int i = FRAME_W / 2; i--;)
		{
			int li = i << 1;

			luma_row[li + 0] = row[i] & 0xFF;
			luma_row[li + 1] = (row[i] >> 16) & 0xFF;

			chroma_row[i].cr = (row[i] >> 8) & 0xFF;
			chroma_row[i].cb = (row[i] >> 24) & 0xFF;

		}
	}


	return 0;
}

/**
 * @brief Gets the min and max throttle and steering values. Saves updated
 *        max and min values automatically.
 * @returns 0 on success
 */
int calibration(cam_settings_t cfg)
{
	raw_action_t action = {};

	poll_i2c_devs(NULL, &action, NULL);

	CAL.throttle.min = CAL.throttle.max = action.throttle;
	CAL.steering.min = CAL.steering.max = action.steering;

	for (int i = 1000; i--;)
	{
		poll_i2c_devs(NULL, &action, NULL);
		calib_t last_cal = CAL;

		if (action.steering > CAL.steering.max)
			CAL.steering.max = action.steering;
		if (action.throttle > CAL.throttle.max)
			CAL.throttle.max = action.throttle;

		if (action.steering < CAL.steering.min)
			CAL.steering.min = action.steering;
		if (action.throttle < CAL.throttle.min)
			CAL.throttle.min = action.throttle;

		if (memcmp(&last_cal, &CAL, sizeof(CAL)))
		{
			b_log("throttle: [%f - %f]\nsteering: [%f - %f]",
					CAL.throttle.min, CAL.throttle.max,
					CAL.steering.min, CAL.steering.max
			       );

			// save the calibration profile
			int fd = open(ACTION_CAL_PATH, O_CREAT | O_WRONLY, 0666);

			if (fd < 0)
			{
				return -2;
			}

			write(fd, &CAL, sizeof(CAL));
			close(fd);
		}

		usleep(500000);
	}

	return 0;
}


/**
 * @brief Spawns a new position estimation thread
 * @param ex - Pointer to a message instance
 * @return 0 on success
 */
int start_pose_thread(message_t* msg)
{
	pthread_t pose_thread;
	return pthread_create(&pose_thread, NULL, pose_estimator, (void*)msg);
}

/**
 * @brief Collects training data, pairing sensor input with human driver output
 *        recording does not begin until wheel movement is detected. Collection
 *        ends and the program is terminated when movement stops.
 * @param cam - pointer to v4l2 camera instance
 * @return 0 on success
 */
int collection(cam_t* cam)
{
	int updates = 0;
	time_t now;
	message_t msg = {
		.header = {
			.magic = MAGIC,
			.type  = PAYLOAD_STATE
		},
	};
	raw_state_t* state = &msg.payload.state;

	pthread_mutex_init(&STATE_LOCK, NULL);
	now = time(NULL);
	start_pose_thread(&msg);

	// burn the first frames
	for (int i = 15; i--;)
	{
		cam_request_frame(cam);
		poll_vision(state, cam);
	}

	// wait for the bot to start moving
	if (cli_cfg.wait_for_movement)
	while (state->vel <= 0)
	{
		usleep(100000);
	}

	for (;;)
	{
		cam_request_frame(cam);

		// Do something while we wait for our
		// next frame to come in...
		++updates;


		if (poll_vision(state, cam))
		{
			b_bad("Error capturing frame");
			return -2;
		}

		pthread_mutex_lock(&STATE_LOCK);

		if (now != time(NULL))
		{
			LOG_LVL(1) b_log("%dHz (%f %f %f) %fm/s %fm",
				updates,
				state->position[0],
				state->position[1],
				state->position[2],
				state->vel,
				state->distance
			);

			LOG_LVL(3) b_log("act: t: %d, s: %d",
				(int)msg.payload.pair.action.throttle,
				(int)msg.payload.pair.action.steering
			);

			updates = 0;
			now = time(NULL);
		}

		if (write_pipeline_payload(&msg))
		{
			b_bad("Error writing state-action pair");
			return -3;
		}
		pthread_mutex_unlock(&STATE_LOCK);


		if (state->vel == 0 && cli_cfg.wait_for_movement)
		{
			exit(0);
		}

		int killswitch = msg.payload.pair.action.throttle < 114 && cli_cfg.read_action;
		int finished = cfg.meters_to_travel > 0 && state->distance > cfg.meters_to_travel;

		if (killswitch || finished)
		{
			// terminated
			exit(1);
		}
	}
}

/**
 * @brief Generates random data rather than collecting it from sensors
 * @return 0 on success
 */
int random_data()
{
	int fd_rnd = open("/dev/random", O_RDONLY);
	int updates = 0;
	time_t now;
	message_t msg = {
		.header = {
			.magic = MAGIC,
			.type  = PAYLOAD_STATE
		},
	};
	raw_state_t* state = &msg.payload.state;

	if (fd_rnd < 0)
	{
		return -1;
	}

	for (;;)
	{
		// Do something while we wait for our
		// next frame to come in...
		++updates;
		if (now != time(NULL))
		{
			b_log("%dHz (%f %f %f) %fm/s",
				updates,
				state->position[0],
				state->position[1],
				state->position[2],
				state->vel
			);
			updates = 0;
			now = time(NULL);
		}

		write(fd_rnd, state, sizeof(raw_state_t));

		if (write_pipeline_payload(&msg))
		{
			b_bad("Error writing state-action pair");
			return -3;
		}
	}
}


int main(int argc, char* const argv[])
{
	PROC_NAME = argv[0];
	proc_opts(argc, argv);

	cfg_base("/etc/bot/collector/");
	load_cfg();

	int res;
	cam_settings_t cfg = {
		.width  = FRAME_W,
		.height = FRAME_H,
		.frame_rate = cli_cfg.frame_rate,
	};

	b_log("Sensors...");

	if ((res = i2c_init("/dev/i2c-1")))
	{
		b_bad("I2C init failed (%d)", res);

		close(I2C_BUS);
		I2C_BUS = -1;

		//return -1;
	}
	pwm_reset_soft();

	cam_t cam[2] = {
		cam_open("/dev/video0", &cfg),
		//cam_open("/dev/video1", &cfg),
	};

	// Use the round-robin real-time scheduler
	// with a high priority
	struct sched_param sch_par = {
		.sched_priority = 50,
	};

	if (sched_setscheduler(0, SCHED_RR, &sch_par) != 0)
	{
		b_bad("RT-scheduling not set");
	}

	// Check to see if that action calibration file
	// exists. If not, switch to calibration mode. Otherwise
	// load the calibration values
	struct stat st;
	if (stat(ACTION_CAL_PATH, &st))
	{
		MODE = COL_MODE_ACT_CAL;
		b_bad("No %s file found. Calibrating...", ACTION_CAL_PATH);
		calibration(cfg);
	}
	else
	{
		assert(calib_load(ACTION_CAL_PATH, &CAL) == 0);
	}

	if (cli_cfg.gen_random)
	{
		res = random_data();
	}
	else
	{
		res = collection(cam);
	}

	return res;
}
