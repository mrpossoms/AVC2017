#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <spawn.h>
#include <sys/wait.h>
#include <stdarg.h>

#include "i2c.h"
#include "drv_pwm.h"

static const char* MEDIA_PATH;
static int RUNNING;
static int DAEMONIZE;
static FILE* LOG_FILE;

#define GOOD(fmt, ...) do { log_msg(".", fmt, __VA_ARGS__); } while(0)
#define BAD(fmt, ...) do { log_msg("!", fmt, __VA_ARGS__); } while(0)
#define INFO(fmt, ...) do { log_msg("*", fmt, __VA_ARGS__); } while(0)

struct {
	int last_odo;
} app = {};


static void log_msg(const char* type, const char* fmt, ...)
{
	va_list ap;
	char buf[1024];

	fprintf(LOG_FILE, "botd [%ld] %s: ", time(NULL), type);

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	
	fprintf(LOG_FILE, "%s\n", buf);
}


static void bad(const char* fmt, ...)
{
	va_list ap;
	char buf[1024];

	log_msg("!", fmt);

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	
	fprintf(LOG_FILE, "%s\n", buf);
}


void proc_opts(int argc, char* const argv[])
{
	int c;
	while ((c = getopt(argc, argv, "dm:")) != -1)
	{
		switch(c)
		{
			case 'd':
				DAEMONIZE = 1;
				break;
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
		BAD("i2c_init failed (%d)\n", res);
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
	
	LOG_FILE = fopen("/var/log/botd", "w+");
	
	if(!LOG_FILE)
	{
		fprintf(stderr, "Failed to open logfile (%d)\n", errno);
		return -1;
	}

	if(!MEDIA_PATH) 
	{
		BAD("Please provide a training data media path\n");
		return -1;	
	}

	i2c_up();

	RUNNING = 1;

	if(DAEMONIZE)
	{
		if(fork() != 0) return 0;
	}

	printf("I'm the child %d\n", RUNNING);
	// child
	while(RUNNING) child_loop();

	return 0;
}
