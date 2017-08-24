#ifndef AVC_CURVES
#define AVC_CURVES

#include "structs.h"

int   bucket_index(float value, range_t* buckets_range, int buckets);
void  dataset_mean(example_t* dataset, float* mu_vec, unsigned int dims, unsigned int examples);
float falloff(float mu, float x);
void  convert_action(raw_action_t* ra, action_f_t* a, calib_t* cal);

#endif
