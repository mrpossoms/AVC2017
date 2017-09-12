#ifndef AVC_SYS
#define AVC_SYS

#include <sys/types.h>
#include <sys/time.h>
#include <inttypes.h>
#include <pthread.h>
#include <math.h>

typedef struct {	
	struct timeval start;
	uint32_t interval_us;
} timegate_t;

long diff_us(struct timeval then, struct timeval now);
void timegate_open(timegate_t* tg);
void timegate_close(timegate_t* tg);

#endif
