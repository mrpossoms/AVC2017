#include <stdlib.h>
#include <stdio.h>

#include "structs.h"

typedef struct {
	float min, max;
} range_t;


void dataset_mean(float* dataset, float* mu_vec, unsigned int dims, unsigned int examples)
{
	bzero(mu_vec, sizeof(float) * dims);

	for(int i = examples; i--;)
	{
		float* ex = dataset + (dims * i);

		for(int j = dims; j--;)
		{
			mu_vec[j] += ex[j];
		}
	}

	for(int j = dims; j--;)
	{
		mu_vec[j] /= (float)examples;
	}
}

void dataset_range(float* dataset, range_t* ranges, unsigned int dims, unsigned int examples)
{
	float* ex = dataset;

	for(int j = dims; j--;)
	{
		ranges[j].min = ex[j];
		ranges[j].max = ex[j];
	}

	for(int i = examples; i--;)
	{
		ex = dataset + (dims * i);

		for(int j = dims; j--;)
		{
			float f = ex[j];
			if(f > ranges[j].max) ranges[j].max = f;
			if(f < ranges[j].min) ranges[j].min = f;
		}
	}
}

void dataset_raw_to_float(raw_state_t* raw_ex, state_f_t* ex, unsigned int examples)
{
	for(int i = examples; i--;)
	{
		raw_state_t* rs = raw_ex + i;
		state_f_t*   s  = ex + i;

		// Convert IMU vectors
		for(int j = 3; j--;)
		{
			s->rot[j] = rs->rot_rate[j];
			s->acc[j] = rs->acc[j];
		}

		// Convert ODO values
		s->vel = rs->vel;
		s->distance = rs->distance;

		// Convert camera data
		uint32_t* yuvy = rs->view;
		for(int cell = sizeof(rs->view) / sizeof(uint32_t); cell--;)
		{
			s->chroma[cell] = (yuvy[cell] >> 8) & 0xFFFF;
			s->luma[(cell << 1) + 0] = yuvy[cell] >> 24;
			s->luma[(cell << 1) + 1] = yuvy[cell] & 0x08;
		}
	}
}

int main(int argc, const char* argv[])
{
	

	return 0;
}
