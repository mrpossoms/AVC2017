#define _GNU_SOURCE

#include "sys.h"
#include "structs.h"
#include "dataset_hdr.h"
#include "i2c.h"
#include "drv_pwm.h"
#include "cam.h"
#include "linmath.h"
#include "deadreckon.h"

typedef enum {
	COL_MODE_NORMAL = 0,
	COL_MODE_ACT_CAL,
	COL_MODE_ROUTE,
} col_mode_t;

int I2C_BUS;
int NORM_VIDEO;
int WAIT_FOR_MOVEMENT = 1;
int READ_ACTION = 1;
int FRAME_RATE = 30;
char* MEDIA_PATH;
calib_t CAL;
col_mode_t MODE;
float VEL;
pthread_mutex_t STATE_LOCK;

/**
 * @brief Sets program state according to specified command line flags
 * @param argc - number of arguments
 * @param argv - array of character pointers to arguments.
 */
void proc_opts(int argc, const char ** argv)
{
	int no_opts = 1;

	const char* cmds = "?cniaf:";
	const char* prog_desc = "Collects data from sensors, compiles them into system state packets. Then forwards them over stdout";
	const char* cmd_desc[] = {
		"Show this help",
		"Calibrate action vector (steering and throttle)",
		"Normalize video frames",
		"Start immediately, don't wait for movement",
		"disable polling of PWM action values",
		"set framerate in frames/second",
	};

	for (;;)
	{
		int c = getopt(argc, (char *const *)argv, cmds);
		if (c == -1) break;

		switch (c)
		{
			case '?': // Display usage help
			cli_help(prog_desc, cmds, cmd_desc);
			case 'c': // Calibrate
				MODE = COL_MODE_ACT_CAL;
				b_log("Calibrating action vector");
				break;
			case 'n': // Normalize video
				NORM_VIDEO = 1;
				break;
			case 'i': // Start immediately
				WAIT_FOR_MOVEMENT = 0;
				break;
			case 'a':
				READ_ACTION = 0;
				break;
			case 'f':
				FRAME_RATE = atoi(optarg);
				break;
		}
	}
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

	range_t luma_range = { 128, 128 };
	range_t cr_range = { 128, 128 };
	range_t cb_range = { 128, 128 };
	float luma_mu = 0;
	float cr_mu = 0;
	float cb_mu = 0;

	raw_state_t new_frame = {};

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
 * @param ex - Pointer to raw example pointer
 * @return 0 on success
 */
int start_pose_thread(raw_example_t* ex)
{
	pthread_t pose_thread;
	return pthread_create(&pose_thread, NULL, pose_estimator, (void*)ex);
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
	int res = 0, fd = 1;
	int started = 0, updates = 0;
	time_t now;
	playload playload = {
		.header = {
			.magic = MAGIC,
			.type  = PAYLOAD_STATE
		},
	};
	raw_state_t* state = &payload.input;

	pthread_mutex_init(&STATE_LOCK, NULL);

	if (READ_ACTION)
	{
		//pwm_set_echo(0x6);
	}

	now = time(NULL);

	start_pose_thread(state);

	// wait for the bot to start moving
	if (WAIT_FOR_MOVEMENT)
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

		if (poll_vision(state, cam))
		{
			b_bad("Error capturing frame");
			return -2;
		}

		pthread_mutex_lock(&STATE_LOCK);
		if (write_pipeline_payload(&payload))
		{
			b_bad("Error writing state-action pair");
			return -3;
		}
		pthread_mutex_unlock(&STATE_LOCK);


		if (state->vel == 0 && WAIT_FOR_MOVEMENT)
		{
			exit(0);
		}
	}
}


int main(int argc, const char* argv[])
{
	PROC_NAME = argv[0];
	proc_opts(argc, argv);

	int res;
	cam_settings_t cfg = {
		.width  = 160,
		.height = 120,
		.frame_rate = FRAME_RATE,
	};

	b_log("Sensors...");

	cam_t cam[2] = {
		cam_open("/dev/video0", &cfg),
		//cam_open("/dev/video1", &cfg),
	};


	if ((res = i2c_init("/dev/i2c-1")))
	{
		b_bad("I2C init failed (%d)", res);

		close(I2C_BUS);
		I2C_BUS = -1;

		return -1;
	}

	if (READ_ACTION)
	{
		pwm_set_echo(0x6);
	}

	pwm_reset_soft();
	b_good("OK");

	// Use the round-robin real-time scheduler
	// with a high priority
	struct sched_param sch_par = {
		.sched_priority = 50,
	};
	assert(sched_setscheduler(0, SCHED_RR, &sch_par) == 0);

	// Check to see if that action calibration file
	// exists. If not, switch to calibration mode. Otherwise
	// load the calibration values
	struct stat st;
	if (stat(ACTION_CAL_PATH, &st))
	{
		MODE = COL_MODE_ACT_CAL;
		b_bad("No %s file found. Calibrating...", ACTION_CAL_PATH);
	}
	else
	{
		assert(calib_load(ACTION_CAL_PATH, &CAL) == 0);
	}

	res = collection(cam);


	return res;
}
