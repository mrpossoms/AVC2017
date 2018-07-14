#define _GNU_SOURCE
#include <sched.h>

#include "sys.h"
#include "structs.h"
#include "i2c.h"
#include "drv_pwm.h"
#include "linmath.h"
// #include "curves.h"
#include "deadreckon.h"

extern int READ_ACTION;
int POSE_CYCLES;

void* pose_estimator(void* params)
{
	timegate_t tg = {
		.interval_us = 10000
	};

	message_t* msg = (message_t*)params;
	raw_state_t* state = &msg->payload.state;
	raw_action_t* act_ptr = NULL;

#ifdef __linux__
#warning "Using CPU affinity"
	// Run exclusively on the 4th core
	cpu_set_t* pose_cpu = CPU_ALLOC(1);
	CPU_SET(3, pose_cpu);
	size_t pose_cpu_size = CPU_ALLOC_SIZE(1);
	assert(sched_setaffinity(0, pose_cpu_size, pose_cpu) == 0);
#endif

	int LAST_D_ODO_CYCLE = 0;
	int last_odo = 0;

	if(READ_ACTION)
	{
		act_ptr = &msg->payload.action;
	}

	// terminate if the I2C bus isn't open
	if(I2C_BUS < 0)
	{
		return (void*)-1;
	}

	while(1)
	{
		timegate_open(&tg);

		int odo = 0;
		struct bno055_quaternion_t iq;

		pthread_mutex_lock(&STATE_LOCK);

		if(poll_i2c_devs(state, READ_ACTION ? act_ptr : NULL, &odo))
		{
			return (void*)-1;
		}

		const float wheel_cir = 0.082 * M_PI / 4.0;
		float delta = (odo - last_odo) * wheel_cir;
		int cycles_d = POSE_CYCLES - LAST_D_ODO_CYCLE;

		if(delta)
		{
			state->vel = delta / (cycles_d * (tg.interval_us / 1.0E6));
			LAST_D_ODO_CYCLE = POSE_CYCLES;
		}

		if(cycles_d * tg.interval_us > 1E6)
		{
			state->vel = 0;
		}

		// TODO: pose integration
		bno055_read_quaternion_wxyz(&iq);
		const float m = 0x7fff >> 1;
		vec3 forward = { 0, 1, 0 };
		vec3 heading;
		quat q = { iq.x / m, iq.y / m, iq.z / m, iq.w / m };
		quat_mul_vec3(heading, q, forward);
		heading[2] = 0;
		vec3_norm(heading, heading);

		// Essentially, apply a lowpass filter
		float p = (vec3_mul_inner(heading, state->heading) + 1) / 2;

		if(vec3_len(state->heading) < 0.9f)
		{
			p = 1;
		}

		vec3_lerp(state->heading, state->heading, heading, powf(p, 64));

		vec3_scale(heading, state->heading, delta);
		vec3_add(state->position, state->position, heading);

		state->distance += delta;

		pthread_mutex_unlock(&STATE_LOCK);

		POSE_CYCLES++;
		last_odo = odo;
		timegate_close(&tg);
	}
}
