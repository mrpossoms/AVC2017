#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <spawn.h>
#include <sys/wait.h>
#include <time.h>
#include <stdarg.h>

#include "sys.h"
#include "i2c.h"
#include "drv_pwm.h"
#include "cfg.h"

static int RUNNING;
static int DAEMONIZE;

#define LED_PATH "/sys/class/leds/led1/brightness"

void set_led(int on)
{
	int fd;
	write((fd = open(LED_PATH, O_WRONLY)), on ? "1" : "0", 1);
	close(fd);
}


static void i2c_up()
{
	int res = i2c_init("/dev/i2c-1");

	if(res)
	{
		b_bad("i2c_init failed (%d)\n", res);
		exit(-1);
	}

	pwm_set_echo(0x6);
}

int RUN_MODE = 1;
pid_t BOT_JOB_PID = 0;

int main(int argc, char* const argv[])
{
	// define and process cli args
	cli_cmd_t cmds[] = {
		CLI_CMD_LOG_VERBOSITY,
		{} // terminator
	};
	cli("Runs in the background and automatically executes pipelines of robot programs",
	cmds, argc, argv);

	cfg_base("/etc/bot/botd/");

	struct {
		float x, y, z;
	} filter = {};

	int cooldown = 100;

	PROC_NAME = argv[0];

	i2c_up();
	set_led(RUN_MODE);

	RUNNING = 1;

	if(DAEMONIZE)
	{
		if(fork() != 0) return 0;
	}

	// b_log("I'm the child %d\n", RUNNING);
	b_log("mode: run");

	// child
	while(RUNNING)
	{
		raw_state_t state;
		raw_action_t act;

		if (poll_i2c_devs(&state, &act, NULL))
		{
			b_bad("poll_i2c_devs() - failed");
			sleep(1);
			continue;
		}

		if (!act.throttle || !act.throttle)
		{
			continue;
		}

		if (BOT_JOB_PID == 0)
		{
			filter.x = (state.acc[0] + filter.x * 9) / 10.f;
			filter.y = (state.acc[1] + filter.y * 9) / 10.f;
			filter.z = (state.acc[2] + filter.z * 9) / 10.f;
		}

		// b_log("r: %d, %d, %d - f: %d, %d, %d", state.acc[0], state.acc[1], state.acc[2], filter.x, filter.y, filter.z);
		LOG_LVL(3) b_log("t: %d, s: %d", act.throttle, act.steering);

		if(filter.x > 512 && RUN_MODE == 0)
		{
			RUN_MODE = 1;
			LOG_LVL(2) b_log("mode: run");
		}
		else if(filter.x < -512 && RUN_MODE == 1)
		{
			RUN_MODE = 0;
			LOG_LVL(2) b_log("mode: record");
		}

		set_led(RUN_MODE);

		if(act.throttle > 118 && !BOT_JOB_PID)
		{ // run route
			char** argv = NULL;

			if (RUN_MODE)
			{
				LOG_LVL(1) b_log("Running!\n");
				// i2c_uninit();
				static char* argv_run[] = { "sh", "-c", "", NULL };
				argv_run[2] = cfg_str("run_cmd", "collector -ia | predictor -f | actuator -m2 -f > /var/testing/route");
				argv = argv_run;
			}
			else
			{
				LOG_LVL(1) b_log("Recording!\n");

				static char* argv_train[] = { "sh", "-c", "", NULL };
				argv_train[2] = cfg_str("record_cmd", "collector -ia > /var/training/route");
				argv = argv_train;
			}

			// posix_spawn(
			// 	&BOT_JOB_PID,
			// 	"/bin/sh",
			// 	NULL, NULL,
			// 	argv,
			// 	NULL
			// );
			LOG_LVL(2) b_log("running: '%s'", argv[2]);
			system(argv[2]);
			LOG_LVL(1) b_log("%d started", BOT_JOB_PID);
		}
		else if(act.throttle < 110 && BOT_JOB_PID)
		{
			// if (kill(BOT_JOB_PID, 2))
			// {
			// 	b_bad("Signalling processes to terminate failed");
			// }

			//  waitpid(BOT_JOB_PID, NULL, 0);
			system("killall -s2 actuator collector");

			LOG_LVL(0) b_good("%d Run finished", act.throttle);
			BOT_JOB_PID = 0;
		}

		cooldown--;
		if (cooldown < 0) { cooldown = 0; }

		usleep(1000 * 50);
	}

	return 0;
}
