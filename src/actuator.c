#include <stdio.h>
#include <stdarg.h>
#include "sys.h"
#include "structs.h"
#include "i2c.h"
#include "drv_pwm.h"
#include "pid.h"


int INPUT_FD = 0;
int FORWARD_STATE = 0;
int I2C_BUS;

calib_t CAL;
uint8_t PWM_CHANNEL_MSK = 0x6; // all echo

time_t LAST_SECOND;


void proc_opts(int argc, char* const *argv)
{
	const char* cmds = "hfm:";
	const char* prog_desc = "Recieves action vectors over stdin and actuates the platform";
	const char* cmd_desc[] = {
		"Show this help",
		"Forward full system state over stdout",
		"Mask PWM output channels. Useful for disabling throttle or steering",
	};

	int c;
	while((c = getopt(argc, argv, cmds)) != -1)
	switch(c)
	{
		case 'h':
			cli_help(argv, prog_desc, cmds, cmd_desc);
		case 'f':
			FORWARD_STATE = 1;
			break;
		case 'm':
			// Explicit PWM channel masking
			PWM_CHANNEL_MSK = atoi(optarg);
			b_log("channel mask %x", PWM_CHANNEL_MSK);
			break;
	}
}


void sig_handler(int sig)
{
	b_log("Caught signal %d", sig);
	pwm_set_echo(0x6);
	usleep(10000);
	exit(0);
}


int main(int argc, char* const argv[])
{
	PROC_NAME = argv[0];

	signal(SIGINT, sig_handler);

	if (calib_load(ACTION_CAL_PATH, &CAL))
	{
		b_log("Failed to load '%s'", ACTION_CAL_PATH);
		return -1;
	}

	proc_opts(argc, argv);

	if (i2c_init("/dev/i2c-1"))
	{
		b_log("Failed to init i2c bus");
		I2C_BUS = -1;
		// return -2;
	}
	else
	{
		pwm_set_echo(PWM_CHANNEL_MSK);
	}

	while(1)
	{
		message_t msg = {};

		if (!read_pipeline_payload(&msg, PAYLOAD_PAIR))
		{
			if (I2C_BUS > -1)
			{
				pwm_set_action(&msg.payload.action);
			}
			else
			{
				static int sim_pipe;
				if (sim_pipe <= 0)
				{
					sim_pipe = open("./avc.sim.ctrl", O_WRONLY);
					b_log("Opened pipe");
				}
				else
				{
					write(sim_pipe, &msg.payload.action, sizeof(msg.payload.action));
				}
			}

			if (FORWARD_STATE)
			{
				if (write_pipeline_payload(&msg))
				{
					b_bad("Failed to write payload");
					return -1;
				}
			}


		}
		else
		{
			b_bad("read error");
			if (I2C_BUS > -1)
			{
				// stop everything
				raw_action_t act = { 117, 117 };
				pwm_set_action(&act);
			}

			return -1;
		}
	}

	b_bad("terminating");

	return 0;
}
