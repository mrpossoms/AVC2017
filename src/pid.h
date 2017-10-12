#ifndef AVC_PID
#define AVC_PID

typedef struct {
	float p, i, d;  // controller coefficents
	float integrated_e;
	float last_error;
} PID_t;

static inline float PID_control(PID_t* pid, float target, float actual)
{
	float e = target - actual;
	float de = pid->last_error - e;
	
	pid->integrated_e += e;
	pid->last_error = e;

	return e * pid->p + pid->i * pid->integrated_e + pid->d * de;
}

#endif
