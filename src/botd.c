#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <spawn.h>

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

		if(act.throttle - 1 > THROTTLE_STOPPED || act.throttle + 1 < THROTTLE_STOPPED)
		{
			//
			// Start collecting!
			//
			pid_t collector_pid;
			char buf[1024];
			char* argv[1];

			snprintf(buf, sizeof(buf), "-m%s", MEDIA_PATH);
			argv[0] = buf;
			posix_spawn(
				&collector_pid, 
				"collector",
				NULL, NULL,
				argv,
				NULL
			);
		}	
	}
}


int main(int argc, char* const argv[])
{
	int res;
	proc_opts(argc, argv);
	
	if(!MEDIA_PATH) return -1;	
	
	if((res = i2c_init("/dev/i2c-1")))
	{
		fprintf(stderr, "i2c_init failed (%d)\n", res);
		return -1;
	}

	RUNNING = 1;
	pid_t child_pid = fork();

	if(child_pid < 0) return -2;
	if(child_pid == 0)
	{
		// child
		while(RUNNING) child_loop();
	}


	return 0;
}
