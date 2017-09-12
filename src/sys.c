#include "sys.h"

#include <unistd.h>


void timegate_open(timegate_t* tg)
{
	gettimeofday(&tg->start, NULL);
}

long diff_us(struct timeval then, struct timeval now)
{
	long us = (now.tv_sec - then.tv_sec) * 10e6;
	us -= (then.tv_usec - now.tv_usec);

	if(us < 0) return 0;

	return us;
}

void timegate_close(timegate_t* tg)
{
	struct timeval now = {};
	gettimeofday(&now, NULL);

	long residual = diff_us(tg->start, now);
	usleep(tg->interval_us - residual);
}
