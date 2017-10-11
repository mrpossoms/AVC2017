#ifndef AVC_SYS
#define AVC_SYS

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <getopt.h>
#include <sched.h>
#include <pthread.h>

#include "structs.h"

#define ACTION_CAL_PATH "actions.cal"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

extern const char* PROC_NAME;

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
