#include <unistd.h>
#include "drv_pwm.h"
#include "i2c.h"

static int NO_ECHO_MODE;

int pwm_get_action(raw_action_t* action)
{
	if(!action) return 1;

	// Get throttle and steering state
	if(i2c_read(I2C_BUS_FD, PWM_LOGGER_ADDR, 2, (void*)action, sizeof(raw_action_t)))
	{
		return 2;
	}

	return 0;
}


int pwm_set_echo(uint8_t is_echo_flags)
{
	i2c_write(I2C_BUS_FD, PWM_LOGGER_ADDR, 0x0, is_echo_flags);
	usleep(50000); // wait 2 PWM cycles
}


int pwm_set_action(raw_action_t* action)
{
	if(!action) return 1;

	if(i2c_write_bytes(I2C_BUS_FD, PWM_LOGGER_ADDR, 2, (uint8_t*)action, sizeof(raw_action_t)))
	{
		return 2;	
	}

	return 0;
}


int pwm_reset()
{
	// reboot the PWM logger
	i2c_write(I2C_BUS_FD, PWM_LOGGER_ADDR, 0x0B, 0);
	sleep(2);

	return 0;
}


int pwm_get_odo()
{
	uint16_t odo;
	int res = i2c_read(I2C_BUS_FD, PWM_LOGGER_ADDR, 0x0C, (void*)&odo, sizeof(odo));

	if(res) return -1;
	return odo;
}
