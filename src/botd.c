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
static FILE* LOG_FILE;

#define GOOD(fmt...) do { log_msg(".", fmt); } while(0)
#define BAD(fmt...) do { log_msg("!", fmt); } while(0)
#define INFO(fmt...) do { log_msg("*", fmt); } while(0)

#define LED_PATH "/sys/class/leds/led0/brightness"

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


void set_led(int on)
{
	int fd;
	write((fd = open(LED_PATH, O_WRONLY)), on ? "1" : "0", 1);
	close(fd);
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

	pwm_set_echo(0x6);
}

int RECORD_MODE = 1;
int DX;

void child_loop()
{
	int odo = pwm_get_odo();
	static int last_record_mode;
	struct bno055_accel_t acc = {};

	bno055_read_accel_xyz(&acc);

	DX = (DX * 9 + acc.x) / 10;

	if(DX > 256 && RECORD_MODE == 1)
	{
		RECORD_MODE = 0;
	}
	else if(DX < -256 && RECORD_MODE == 0)
	{
		RECORD_MODE = 1;
	}
	else if(abs(acc.y) > 512)
	{ // run route
		b_log("Running!\n");
		i2c_uninit();
		system("collector -i -a | predictor -r/media/training/0.route -m2");
		i2c_up();
		b_log("Run finished\n");
	}

	if(last_record_mode != RECORD_MODE)
	{
		set_led(RECORD_MODE);
	}

	if(odo > app.last_odo)
	{ // Start recording session
		write(1, ".", 1);

		raw_action_t act = {};
		if(pwm_get_action(&act))
		{
			exit(-2);
		}

		if(act.throttle > 0)
		if(act.throttle - 1 > THROTTLE_STOPPED || act.throttle + 1 < THROTTLE_STOPPED)
		{
			INFO("Recording session...");

			//
			// Let the collector have the i2c bus
			//
			i2c_uninit();

			//
			// Start collecting!
			//
			pid_t collector_pid;
			char* argv[] = { "collector", "-m", MEDIA_PATH, "-r", NULL };

			if(RECORD_MODE == 0)
			{
				argv[3] = NULL;
			}

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

			INFO("Session finished");
			INFO("Waiting...");
			i2c_up();
		}
	}

	app.last_odo = odo;
	usleep(1000 * 50);
}


int main(int argc, char* const argv[])
{
	PROC_NAME = argv[0];
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
