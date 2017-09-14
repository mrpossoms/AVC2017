#include "sys.h"

#include <unistd.h>
#include <stdio.h>


void timegate_open(timegate_t* tg)
{
	gettimeofday(&tg->start, NULL);
}

long diff_us(struct timeval then, struct timeval now)
{
	long us = (now.tv_sec - then.tv_sec) * 1e6;

	if(us)
	{
		us = (us - then.tv_usec) + now.tv_usec;
	}
	else
	{
		us = now.tv_usec - then.tv_usec;
	}

	return us;
}

void timegate_close(timegate_t* tg)
{
	struct timeval now = {};
	gettimeofday(&now, NULL);

	long residual = diff_us(tg->start, now);
	residual = tg->interval_us - residual;

	if(residual < 0) return;	
	usleep(residual);
}
