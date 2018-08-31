#include <stdio.h>
#include <stdarg.h>
#include "sys.h"
#include "structs.h"
#include "i2c.h"
#include "drv_pwm.h"
#include "pid.h"
#include "cfg.h"

#define LOG_LVL(n) if (LOG_VERBOSITY >= (n))

int INPUT_FD = 0;
int I2C_BUS = 0;

calib_t CAL = {};

struct {
	uint8_t pwm_channel_msk; // all echo
	int forward_state;
} cli_cfg = {
	.pwm_channel_msk = 0x6,
	.forward_state = 0,
};


void sig_handler(int sig)
{
	b_log("Caught signal %d", sig);
	pwm_set_echo(0xff);
	exit(0);
}


int main(int argc, char* const argv[])
{
	PROC_NAME = argv[0];

	cfg_base("/etc/bot/actuator/");

	int max_throttle = cfg_int("max-throttle", 130);
	int min_throttle = cfg_int("min-throttle", 117);

	signal(SIGINT, sig_handler);

	if (calib_load(ACTION_CAL_PATH, &CAL))
	{
		b_log("Failed to load '%s'", ACTION_CAL_PATH);
		return -1;
	}

	// define and process cli args
	cli_cmd_t cmds[] = {
		{ 'f',
			.desc = "Forward full system state over stdout",
			.set = &cli_cfg.forward_state,
		},
		{ 'm',
			.desc = "Mask PWM output channels. Useful for disabling throttle or steering",
			.set = &cli_cfg.pwm_channel_msk,
			.type = ARG_TYP_INT,
			.opts = { .has_value = 1 },
		},
		CLI_CMD_LOG_VERBOSITY,
		{} // terminator
	};
	cli("Recieves action vectors over stdin and actuates the platform",
	cmds, argc, argv);

	if (i2c_init("/dev/i2c-1"))
	{
		b_bad("Failed to init i2c bus");
		I2C_BUS = -1;
		// return -2;
	}
	else
	{
		LOG_LVL(1) b_log("setting mask: %x", cli_cfg.pwm_channel_msk);

		if (pwm_set_echo(cli_cfg.pwm_channel_msk))
		{
			b_bad("pwm_set_echo() - failed with msk %x", cli_cfg.pwm_channel_msk);
			return -2;
		}

		LOG_LVL(1) b_log("set mask");
	}

	while(1)
	{
		message_t msg = {};
		raw_action_t act = {};

		if (read_pipeline_payload(&msg, PAYLOAD_PAIR))
		{
			b_bad("read error");
			sig_handler(2);
			return -1;
		}

		act = msg.payload.pair.action;

		LOG_LVL(2) b_log("s:%d t:%d", act.steering, act.throttle);

		if (I2C_BUS > -1)
		{
			float s = act.steering / 256.f;
			float t = msg.payload.action.throttle / 255.f;

			act.steering = CAL.steering.min * (1 - s) + CAL.steering.max * s;
			act.throttle = CAL.throttle.min * (1 - t) + CAL.throttle.max * t;

			if (act.throttle > max_throttle) { act.throttle = max_throttle; }
			if (act.throttle < min_throttle) { act.throttle = min_throttle; }

			if (pwm_set_action(&act))
			{
				b_bad("pwm_set_action() - failed");
			}
		}
		else
		{
			static int sim_pipe;
			if (sim_pipe <= 0)
			{
				sim_pipe = open("./avc.sim.ctrl", O_WRONLY);
				LOG_LVL(1) b_log("Opened pipe");
			}
			else
			{
				write(sim_pipe, &act, sizeof(act));
			}
		}

		if (cli_cfg.forward_state)
		{
			if (write_pipeline_payload(&msg))
			{
				b_bad("Failed to write payload");
				return -1;
			}
		}
	}

	b_bad("terminating");

	sig_handler(0);

	return 0;
}
