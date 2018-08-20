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

extern char** environ;

static const char* MEDIA_PATH;
static int RUNNING;
static int DAEMONIZE;

#define LED_PATH "/sys/class/leds/led0/brightness"

static int LAST_ODO;

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
	int odo_now;
	while(RUNNING)
	{
		raw_state_t state;

		int odo_delta = 0;
		poll_i2c_devs(&state, NULL, &odo_delta);
		odo_now += odo_delta;

		filter.x = (state.acc[0] + filter.x * 9) / 10;
		filter.y = (state.acc[1] + filter.y * 9) / 10;
		filter.z = (state.acc[2] + filter.z * 9) / 10;

		b_log("r: %d, %d, %d - f: %d, %d, %d", state.acc[0], state.acc[1], state.acc[2], filter.x, filter.y, filter.z);

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
		else if(RUN_MODE && abs(filter.y - state.acc[1]) > 512)
		{ // run route
			if (cooldown == 0)
			{
				b_log("Running!\n");
				// i2c_uninit();
				system("collector | predictor -f | actuator -m2 -f > /var/testing/route");
				// i2c_up();
				b_log("Run finished\n");
				cooldown = 100;				
			}
		}

		set_led(RUN_MODE);

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
		// 		pid_t collector_pid;
		// 		char* argv[] = { "collector", "-m", MEDIA_PATH, "-r", NULL };

		// 		if(RUN_MODE == 0)
		// 		{
		// 			argv[3] = NULL;
		// 		}

		// 		// posix_spawn(
		// 		// 	&collector_pid,
		// 		// 	"collector",
		// 		// 	NULL, NULL,
		// 		// 	argv,
		// 		// 	NULL
		// 		// );

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

		LAST_ODO = odo_now;
		usleep(1000 * 50);
	}

	return 0;
}
