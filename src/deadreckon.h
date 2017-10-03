#ifndef AVC_DEAD_RECKON
#define AVC_DEAD_RECKON

#include "sys.h"
#include "structs.h"

extern int POSE_CYCLES;
extern calib_t CAL;
extern pthread_mutex_t STATE_LOCK;

void* pose_estimator(void* params);

#endif
