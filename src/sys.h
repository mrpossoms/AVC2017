#ifndef AVC_SYS
#define AVC_SYS

#include <limits.h>
#include <sys/types.h>
#include <sys/time.h>
#include <inttypes.h>
#include <pthread.h>
#include <math.h>
#include <assert.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <getopt.h>
#include <sched.h>
#include <pthread.h>
#include <sys/stat.h>

#include "structs.h"

#define ACTION_CAL_PATH "actions.cal"

extern char* PROC_NAME;

typedef struct {	
	struct timeval start;
	uint32_t interval_us;
} timegate_t;

void b_log(const char* fmt, ...);

long diff_us(struct timeval then, struct timeval now);
void timegate_open(timegate_t* tg);
void timegate_close(timegate_t* tg);

int calib_load(const char* path, calib_t* cal);

#endif
