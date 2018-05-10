#ifndef AVC_DRV_PWM
#define AVC_DRV_PWM

#include "structs.h"

extern int I2C_BUS;

#define THROTTLE_STOPPED 117

int pwm_reset();
int pwm_reset_soft();
int pwm_get_odo();
int pwm_get_action(raw_action_t* action);
int pwm_set_action(raw_action_t* action);
int pwm_set_echo(uint8_t is_echo_flags);

#endif
