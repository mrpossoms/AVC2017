#define _GNU_SOURCE
#include <sched.h>

#include "sys.h"
#include "structs.h"
#include "i2c.h"
#include "drv_pwm.h"
#include "linmath.h"
#include "curves.h"
#include "deadreckon.h"

extern int READ_ACTION;
int POSE_CYCLES;

void* pose_estimator(void* params)
{
	timegate_t tg = {
		.interval_us = 10000
	};

	raw_example_t* ex = (raw_example_t*)params;
	raw_action_t action, *act_ptr = NULL;	

	// Run exclusively on the 4th core
	cpu_set_t* pose_cpu = CPU_ALLOC(1);
	CPU_SET(3, pose_cpu);
	size_t pose_cpu_size = CPU_ALLOC_SIZE(1);
	assert(sched_setaffinity(0, pose_cpu_size, pose_cpu) == 0);

	float distance_rolled = 0;
	int LAST_D_ODO_CYCLE = 0;
	int last_odo = 0;

	if(READ_ACTION)
	{
		act_ptr = &action;
	}

	while(1)
	{
		timegate_open(&tg);

		int odo = 0;
		struct bno055_quaternion_t iq;		

		if(READ_ACTION)
		{
			if(poll_i2c_devs(&ex->state, act_ptr, &odo))
			{
				return (void*)-1;
			}

			float mu = bucket_index(act_ptr->steering, &CAL.steering, STEERING_BANDS); 
			for(int i = STEERING_BANDS; i--;)
			{
				ex->action.steering[i] = falloff(mu, i);
			}

			mu = bucket_index(act_ptr->throttle, &CAL.throttle, THROTTLE_BANDS); 
			for(int i = THROTTLE_BANDS; i--;)
			{
				ex->action.throttle[i] = falloff(mu, i);
			}
		}

		const float wheel_cir = 0.082 * M_PI / 4.0;
		float delta = (odo - last_odo) * wheel_cir; 
		int cycles_d = POSE_CYCLES - LAST_D_ODO_CYCLE;

		if(delta)
		{
			ex->state.vel = delta / (cycles_d * (tg.interval_us / 1.0E6));
			LAST_D_ODO_CYCLE = POSE_CYCLES;
		}
		
		if(cycles_d * tg.interval_us > 1E6)
		{
			ex->state.vel = 0;
		}

		// TODO: pose integration	
		bno055_read_quaternion_wxyz(&iq);
		const float m = 0x7fff >> 1;
		vec3 forward = { 0, 1, 0 };
		vec3 heading;
		quat q = { iq.x / m, iq.y / m, iq.z / m, iq.w / m };
		quat_mul_vec3(heading, q, forward);

		//pthread_mutex_lock(&STATE_LOCK);
		vec3_copy(ex->state.heading, heading);
		vec3_scale(heading, heading, delta);
		vec3_add(ex->state.position, ex->state.position, heading);
		//pthread_mutex_unlock(&STATE_LOCK);
		POSE_CYCLES++;
		last_odo = odo;
		timegate_close(&tg);
	}
}


