#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <getopt.h>
#include <sched.h>
#include <pthread.h>

#include "sys.h"
#include "structs.h"
#include "dataset_hdr.h"
#include "i2c.h"
#include "drv_pwm.h"
#include "cam.h"
#include "linmath.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

typedef enum {
	COL_MODE_NORMAL = 0,
	COL_MODE_ACT_CAL,
} col_mode_t;

char* MEDIA_PATH;
int I2C_BUS;
int NORM_VIDEO;
col_mode_t MODE;
calib_t CAL;
float VEL;


void proc_opts(int argc, const char ** argv)
{
	for(;;)
	{
		int c = getopt(argc, (char *const *)argv, "cnm:");
		if(c == -1) break;

		switch (c) 
		{
			case 'c':
				MODE = COL_MODE_ACT_CAL;
				fprintf(stderr, "Calibrating action vector\n");
				break;
			case 'n':
				NORM_VIDEO = 1;
				break;
			case 'm':
				MEDIA_PATH = optarg;
				break;
		}
	}
}


int poll_i2c_devs(raw_state_t* state, raw_action_t* action, int* odo)
{
	uint8_t mode = 0;
	int res;

	res = pwm_get_action(action);
	if(res) return res;

	if(odo)
	{
		*odo = pwm_get_odo();
		if(*odo < 0) return *odo;
	}

	res = bno055_get_operation_mode(&mode);

	if(!state) return 3;

	if(bno055_read_accel_xyz((struct bno055_accel_t*)state->acc))
	{
		EXIT("Error reading from BNO055\n");
	}

	if(bno055_read_gyro_xyz((struct bno055_gyro_t*)state->rot_rate))
	{
		EXIT("Error reading from BNO055\n");
	}


	return 0;
}


int poll_vision(raw_state_t* state, cam_t* cams)
{
	cam_wait_frame(cams);

	range_t luma_range = { 128, 128 };
	range_t cr_range = { 128, 128 };
	range_t cb_range = { 128, 128 };
	float luma_mu = 0;
	float cr_mu = 0;
	float cb_mu = 0;


	// Downsample the intensity resolution to match that of
	// the chroma
	for(int j = FRAME_H; j--;)
	{
		int bi = cams[0].buffer_info.index;
		uint32_t* row = cams[0].frame_buffers[bi] + (j * (FRAME_W << 1));
		uint8_t* luma_row = state->view.luma + (j * FRAME_W);
		chroma_t* chroma_row = state->view.chroma + (j * (FRAME_W >> 1));
		
		for(int i = FRAME_W / 2; i--;)
		{
			int li = i << 1;

			luma_row[li + 0] = row[i] & 0xFF;
			luma_row[li + 1] = (row[i] >> 16) & 0xFF;

			chroma_row[i].cr = (row[i] >> 8) & 0xFF;
			chroma_row[i].cb = (row[i] >> 24) & 0xFF;

			if(NORM_VIDEO)
			{
				for(int k = 2; k--;)
				{
					luma_range.min = MIN(luma_range.min, luma_row[li + i + k]);
					luma_range.max = MAX(luma_range.max, luma_row[li + i + k]);
					luma_mu += luma_row[li + i + k];
				}		


				cr_range.min = MIN(cr_range.min, chroma_row[i].cr);
				cr_range.max = MAX(cr_range.max, chroma_row[i].cr);
				cb_range.min = MIN(cb_range.min, chroma_row[i].cb);
				cb_range.max = MAX(cb_range.max, chroma_row[i].cb);

				cr_mu += chroma_row[i].cr;
				cb_mu += chroma_row[i].cb;
			}
		}
	}

	if(NORM_VIDEO)
	{
		luma_mu /= (FRAME_W * FRAME_H);
		cr_mu /= (FRAME_W / 2) * FRAME_H;
		cb_mu /= (FRAME_W / 2) * FRAME_H;
		float luma_spread = luma_range.max - luma_range.min;
		float cr_spread = cr_range.max - cr_range.min;
		float cb_spread = cb_range.max - cb_range.min;

		int bi = cams[0].buffer_info.index;

		for(int j = FRAME_H; j--;)
		{
			uint32_t* row = cams[0].frame_buffers[bi] + (j * FRAME_W * 2);
			uint8_t* luma_row = state->view.luma + (j * FRAME_W);
			chroma_t* chroma_row = state->view.chroma + (j * (FRAME_W >> 1));
			
			for(int i = FRAME_W / 2; i--;)
			{
				int li = i << 1;

				luma_row[li + 0] = 255 * (luma_row[li + 0] - luma_range.min) / luma_spread;
				luma_row[li + 1] = 255 * (luma_row[li + 1] - luma_range.min) / luma_spread;
			}
		}
	}


	return 0;
}


int calibration(cam_settings_t cfg)
{
	raw_action_t action = {};
		
	poll_i2c_devs(NULL, &action, NULL);

	CAL.throttle.min = CAL.throttle.max = action.throttle;
	CAL.steering.min = CAL.steering.max = action.steering;

	for(;;)
	{
		poll_i2c_devs(NULL, &action, NULL);
		calib_t last_cal = CAL;

		if(action.steering > CAL.steering.max) CAL.steering.max = action.steering;
		if(action.throttle > CAL.throttle.max) CAL.throttle.max = action.throttle;

		if(action.steering < CAL.steering.min) CAL.steering.min = action.steering;
		if(action.throttle < CAL.throttle.min) CAL.throttle.min = action.throttle;

		if(memcmp(&last_cal, &CAL, sizeof(CAL)))
		{
			fprintf(stderr, "throttle: [%f - %f]\nsteering: [%f - %f]\n",
					CAL.throttle.min, CAL.throttle.max,
					CAL.steering.min, CAL.steering.max
			       );

			// save the calibration profile
			int fd = open("actions.cal", O_CREAT | O_WRONLY, 0666);
			write(fd, &CAL, sizeof(CAL));
			close(fd);
		}

		usleep(500000);
	}
}


	int last_odo = 0;
pthread_mutex_t STATE_LOCK;
float TIMING = 0;
int CYCLES = 0;
void* pose_estimator(void* params)
{
	timegate_t tg = {
		.interval_us = 10000
	};

	raw_example_t* ex = (raw_example_t*)params;
	struct timeval then, now;
	
	// Run exclusively on the 4th core
	cpu_set_t* pose_cpu = CPU_ALLOC(1);
	CPU_SET(3, pose_cpu);
	size_t pose_cpu_size = CPU_ALLOC_SIZE(1);
	assert(sched_setaffinity(0, pose_cpu_size, pose_cpu) == 0);

	float distance_rolled = 0;
	int LAST_D_ODO_CYCLE = 0;

	while(1)
	{
		gettimeofday(&then, NULL);
		timegate_open(&tg);

		int odo = 0;
		struct bno055_quaternion_t iq;		

		//pthread_mutex_lock(&STATE_LOCK);
		if(poll_i2c_devs(&ex->state, &ex->action, &odo))
		{
			return (void*)-1;
		}
		//pthread_mutex_unlock(&STATE_LOCK);

		const float wheel_cir = 0.082 * M_PI / 4.0;
		float delta = (odo - last_odo) * wheel_cir; 
		int cycles_d = CYCLES - LAST_D_ODO_CYCLE;

		if(delta)
		{
			ex->state.vel = delta / (cycles_d * (tg.interval_us / 1.0E6));
			LAST_D_ODO_CYCLE = CYCLES;
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
		CYCLES++;
		last_odo = odo;
		timegate_close(&tg);
		gettimeofday(&now, NULL);
		TIMING = diff_us(then, now) / 10e6f;
	}
}


int collection(cam_t* cam)
{
	int res = 0, fd = 1;
	pthread_t pose_thread;
	pthread_attr_t pose_attr;
	time_t now;
	int started = 0, updates = 0;
	dataset_header_t hdr = {};
	hdr.magic = MAGIC;
	hdr.is_raw = 1;

	pthread_mutex_init(&STATE_LOCK, NULL);

	if(MEDIA_PATH)
	{
		fd = open(MEDIA_PATH, O_CREAT | O_WRONLY, 0666);

		if(fd < 0)
		{
			fprintf(stderr, "Error: couldn't open/create %s\n", MEDIA_PATH);
			exit(-1);
		}
	}

	// write the header first
	write(fd, &hdr, sizeof(hdr));

	now = time(NULL);
	raw_example_t ex = { };
	int pri = sched_get_priority_max(SCHED_RR) - 1;

	res = pthread_create(&pose_thread, NULL, pose_estimator, (void*)&ex);

	for(;;)
	{
		cam_request_frame(cam);

		// Do something while we wait for our
		// next frame to come in...
		++updates;
		if(now != time(NULL))
		{
			fprintf(stderr, "%dHz (%f %f %f) %d %fm/s %f\n",
				updates,
				ex.state.position[0],
				ex.state.position[1],
				ex.state.position[2],
				last_odo,
				ex.state.vel,
				TIMING
			);
			updates = 0;
			now = time(NULL);
		}

		if(poll_vision(&ex.state, cam))
		{
			fprintf(stderr, "Error capturing frame\n");
			return -2;
		}


		pthread_mutex_lock(&STATE_LOCK);
		if(write(fd, &ex, sizeof(ex)) != sizeof(ex))
		{
			fprintf(stderr, "Error writing state-action pair\n");
			return -3;
		}
		pthread_mutex_unlock(&STATE_LOCK);

	}
}


int main(int argc, const char* argv[])
{
	int res;
	cam_settings_t cfg = {
		.width  = 160,
		.height = 120
	};

	proc_opts(argc, argv);

	fprintf(stderr, "Sensors...");

	cam_t cam[2] = {
		cam_open("/dev/video0", &cfg),
		//cam_open("/dev/video1", &cfg),
	};

	ioctl(cam[0].fd, VIDIOC_S_PRIORITY, V4L2_PRIORITY_RECORD);

	if((res = i2c_init("/dev/i2c-1")))
	{
		fprintf(stderr, "I2C init failed (%d)\n", res);
		//return -1;
	}
	
	pwm_reset();
	fprintf(stderr, "OK\n");

	// Use the round-robin real-time scheduler
	// with a high priority
	struct sched_param sch_par = {
		.sched_priority = 50,
	};
	assert(sched_setscheduler(0, SCHED_RR, &sch_par) == 0);

	switch(MODE)
	{
		case COL_MODE_ACT_CAL:
			calibration(cfg);
			break;
		default:
			res = collection(cam);
	}


	return res;
}
