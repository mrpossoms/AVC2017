#ifndef AVC_SYS
#define AVC_SYS

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


typedef struct {	
	struct timeval start;
	uint32_t interval_us;
} timegate_t;

long diff_us(struct timeval then, struct timeval now);
void timegate_open(timegate_t* tg);
void timegate_close(timegate_t* tg);

#endif
