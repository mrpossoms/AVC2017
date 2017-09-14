#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <errno.h>
#include <strings.h>

#include "curves.h"

int bucket_index(float value, range_t* buckets_range, int buckets)
{
	float d = buckets_range->max - buckets_range->min;

	value -= buckets_range->min;
	return (value / d) * buckets;
}


float falloff(float mu, float x)
{
	float d_mu = x - mu;

	return 1.f / (d_mu * d_mu + 1.f);
}


void convert_action(raw_action_t* ra, action_f_t* a, calib_t* cal)
{
	// Convert the action vectors
	// -------------------------
	int throttle_buckets = sizeof(a->throttle) / sizeof(float);
	int steering_buckets = sizeof(a->steering) / sizeof(float);

	float throttle_mu = bucket_index(
		ra->throttle,
		&cal->throttle,
		throttle_buckets
	);

	float steering_mu = bucket_index(
		ra->steering,
		&cal->steering,
		steering_buckets
	);

	for(int j = throttle_buckets; j--;)
	{
		a->throttle[j] = falloff(throttle_mu, j);
	}

	for(int j = steering_buckets; j--;)
	{
		a->steering[j] = falloff(steering_mu, j);
	}
}
