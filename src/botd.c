#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <spawn.h>
#include <sys/wait.h>

#include "i2c.h"
#include "drv_pwm.h"

static const char* MEDIA_PATH;
static int RUNNING;

struct {
	int last_odo;
} app = {};

void proc_opts(int argc, char* const argv[])
{
	int c;
	while ((c = getopt(argc, argv, "m:")) != -1)
	{
		switch(c)
		{
			case 'm':
				MEDIA_PATH = optarg;
				break;
		}
	}
}


static void i2c_up()
{
	int res = i2c_init("/dev/i2c-1");

	if(res)
	{
		fprintf(stderr, "i2c_init failed (%d)\n", res);
		exit(-1);
	}
}

void child_loop()
{
	int odo = pwm_get_odo();

	if(odo > app.last_odo)
	{
		raw_action_t act = {};
		if(pwm_get_action(&act))
		{
			exit(-2);
		}

		if(act.throttle > 0)
		if(act.throttle - 1 > THROTTLE_STOPPED || act.throttle + 1 < THROTTLE_STOPPED)
		{
			//
			// Let the collector have the i2c bus
			//
			i2c_uninit();

			//
			// Start collecting!
			//
			pid_t collector_pid;
			char buf[1024];
			char* argv[] = { buf, NULL };

			snprintf(buf, sizeof(buf), "-m%s/%lu.session", MEDIA_PATH, time(NULL));
			argv[0] = buf;
			posix_spawn(
				&collector_pid, 
				"collector",
				NULL, NULL,
				argv,
				NULL
			);

			// Wait for the collector process to terminate
			// then bring the i2c bus back up
			waitpid(collector_pid, NULL, 0);
			i2c_up();
		}
	}

	app.last_odo = odo;
}


int main(int argc, char* const argv[])
{
	int res;
	proc_opts(argc, argv);
	
	if(!MEDIA_PATH) return -1;	
	
	i2c_up();

	RUNNING = 1;
	pid_t child_pid = fork();

	if(child_pid < 0) return -2;
	if(child_pid == 0)
	{
		printf("I'm the child %d\n", RUNNING);
		// child
		while(RUNNING) child_loop();
	}


	return 0;
}
