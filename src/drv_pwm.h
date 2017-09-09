#ifndef AVC_DRV_PWM
#define AVC_DRV_PWM

#include "structs.h"

extern int I2C_BUS;

int pwm_reset();
int pwm_get_action(raw_action_t* action);
int pwm_set_action(raw_action_t* action);

#endif
