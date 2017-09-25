#ifndef AVC_DEAD_RECKON
#define AVC_DEAD_RECKON

#include "structs.h"

extern int POSE_CYCLES;
extern calib_t CAL;

void* pose_estimator(void* params);

#endif
