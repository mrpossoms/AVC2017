#include "sys.h"


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


int calib_load(const char* path, calib_t* cal)
{
	int cal_fd = open(ACTION_CAL_PATH, O_RDONLY);

	if(cal_fd < 0)
	{
		return -1;
	}

	if(read(cal_fd, cal, sizeof(calib_t)) != sizeof(calib_t))
	{
		return -2;
	}

	close(cal_fd);
	return 0;
}

const char* PROC_NAME;
void b_log(const char* fmt, ...)
{
	char buf[1024];
	va_list ap;
	
	va_start(ap, fmt);
	vsnprintf(buf, (size_t)sizeof(buf), fmt, ap);
	va_end(ap);
	fprintf(stderr, "[%s] %s (%d)\n", PROC_NAME, buf, errno); 
}


void b_good(const char* fmt, ...)
{
	char buf[1024];
	va_list ap;
	
	va_start(ap, fmt);
	vsnprintf(buf, (size_t)sizeof(buf), fmt, ap);
	va_end(ap);
	fprintf(stderr, AVC_TERM_GREEN "[%s]" AVC_TERM_COLOR_OFF " %s (%d)\n", PROC_NAME, buf, errno); 
}


void b_bad(const char* fmt, ...)
{
	char buf[1024];
	va_list ap;
	
	va_start(ap, fmt);
	vsnprintf(buf, (size_t)sizeof(buf), fmt, ap);
	va_end(ap);
	fprintf(stderr, AVC_TERM_RED "[%s]" AVC_TERM_COLOR_OFF " %s (%d)\n", PROC_NAME, buf, errno); 
}
