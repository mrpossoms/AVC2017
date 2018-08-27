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

#define LOG_LVL(n) if (LOG_VERBOSITY >= (n))

extern char** environ;

static const char* MEDIA_PATH;
static int RUNNING;
static int DAEMONIZE;
static int LOG_VERBOSITY = 0;

#define LED_PATH "/sys/class/leds/led0/brightness"

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
	struct {
		int x, y, z;
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
			filter.x = (state.acc[0] + filter.x * 9) / 10;
			filter.y = (state.acc[1] + filter.y * 9) / 10;
			filter.z = (state.acc[2] + filter.z * 9) / 10;
		}

		// b_log("r: %d, %d, %d - f: %d, %d, %d", state.acc[0], state.acc[1], state.acc[2], filter.x, filter.y, filter.z);
		LOG_LVL(0) b_log("t: %d, s: %d", act.throttle, act.steering);

		if(filter.x > 512 && RUN_MODE == 0)
		{
			RUN_MODE = 1;
			b_log("mode: run");
		}
		else if(filter.x < -512 && RUN_MODE == 1)
		{
			RUN_MODE = 0;
			b_log("mode: record");
		}

		set_led(RUN_MODE);
		
		if(act.throttle > 118 && !BOT_JOB_PID)
		{ // run route
			char** argv = NULL;

			if (RUN_MODE)
			{
				b_log("Running!\n");
				// i2c_uninit();
				static char* argv_run[] = { "sh", "-c", "collector -i | predictor -f | actuator -vvv -m2 -f > /var/testing/route", NULL };
				argv = argv_run;
			}
			else
			{
				static char* argv_train[] = { "sh", "-c", "collector -i > /var/training/route", NULL };
				argv = argv_train;
			}

			posix_spawn(
				&BOT_JOB_PID,
				"/bin/sh",
				NULL, NULL,
				argv,
				NULL
			);
			b_log("%d started", BOT_JOB_PID);
		}
		else if(act.throttle < 110 && BOT_JOB_PID)
		{
			// if (kill(BOT_JOB_PID, 2))
			// {
			// 	b_bad("Signalling processes to terminate failed");
			// }

			//  waitpid(BOT_JOB_PID, NULL, 0);
			system("killall -s2 actuator collector");

			b_log("%d Run finished", act.throttle);
			BOT_JOB_PID = 0;
		}

		// if(odo_now > LAST_ODO)
		// { // Start recording session
		// 	write(1, ".", 1);

		// 	raw_action_t act = {};
		// 	if(pwm_get_action(&act))
		// 	{
		// 		exit(-2);
		// 	}

		// 	if(act.throttle > 0)
		// 	if(act.throttle - 1 > THROTTLE_STOPPED || act.throttle + 1 < THROTTLE_STOPPED)
		// 	{
		// 		b_log("Recording session...");

		// 		//
		// 		// Let the collector have the i2c bus
		// 		//
		// 		// i2c_uninit();

		// 		//
		// 		// Start collecting!
		// 		//
				// pid_t collector_pid;
				// char* argv[] = { "collector", "-m", MEDIA_PATH, "-r", NULL };

				// if(RUN_MODE == 0)
				// {
				// 	argv[3] = NULL;
				// }

				// posix_spawn(
				// 	&collector_pid,
				// 	"collector",
				// 	NULL, NULL,
				// 	argv,
				// 	NULL
				// );

		// 		// // Wait for the collector process to terminate
		// 		// // then bring the i2c bus back up
		// 		// waitpid(collector_pid, NULL, 0);

		// 		b_log("Session finished");
		// 		b_log("Waiting...");
		// 		// i2c_up();
		// 	}
		// }

		cooldown--;
		if (cooldown < 0) { cooldown = 0; }

		usleep(1000 * 250);
	}

	return 0;
}
