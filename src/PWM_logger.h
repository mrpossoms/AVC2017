#ifndef PWM_LOGGER_DRIVER
#define PWM_LOGGER_DRIVER

enum {
	PWM_UNIT_US,
	PWM_UNIT_RAW,
} pwm_unit;

int pwm_write(int* channels, int channel_count, enum pwm_unit unit);
int pwm_read(int* channels, int channel_count, enum pwm_unit unit);

#endif
