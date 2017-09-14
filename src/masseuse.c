#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <errno.h>
#include <strings.h>

#include "structs.h"
#include "curves.h"
#include "dataset_hdr.h"

static calib_t CALIBRATION;
static int DS_SIZE, DS_I_FD, DS_O_FD;
static char *DS_PATH, *DS_O_PATH;
static int DS_FEATURE_SCALE;

void dataset_raw_to_float(raw_example_t* raw_ex, example_t* ex, calib_t* cal)
{
	// Convert the state vectors
	// ------------------------
	// Convert IMU vectors
	for(int j = 3; j--;)
	{
		ex->state.rot[j] = raw_ex->state.rot_rate[j];
		ex->state.acc[j] = raw_ex->state.acc[j];
	}

	// Convert ODO values
	ex->state.vel = raw_ex->state.vel;
	ex->state.distance = raw_ex->state.distance;

	// Convert camera data
	for(int cell = LUMA_PIXELS; cell--;)
	{
		ex->state.view.luma[cell] = raw_ex->state.view.luma[cell];
	}

	for(int cell = CHRO_PIXELS; cell--;)
	{
		ex->state.view.chroma[cell].cr = raw_ex->state.view.chroma[cell].cr;
		ex->state.view.chroma[cell].cb = raw_ex->state.view.chroma[cell].cb;
	}

	// Convert the action vectors
	// -------------------------
	convert_action(&raw_ex->action, &ex->action, cal);
}


void dataset_mean(int fd, float* mu_vec, unsigned int dims, unsigned int examples)
{
	bzero(mu_vec, sizeof(float) * dims);
	lseek(fd, sizeof(dataset_header_t), SEEK_SET);

	for(int i = examples; i--;)
	{
		raw_example_t ex;
		example_t ex_f;
		float* st;

		read(fd, &ex, sizeof(ex));
		dataset_raw_to_float(&ex, &ex_f, &CALIBRATION);
		st = ex_f.state.v;

		for(int j = dims; j--;)
		{
			mu_vec[j] += st[j];
		}
	}

	for(int j = dims; j--;)
	{
		mu_vec[j] /= (float)examples;
	}
}


void dataset_range(int fd, range_t* ranges, unsigned int dims, unsigned int examples)
{
	raw_example_t raw;
	example_t ex;

	lseek(fd, sizeof(dataset_header_t), SEEK_SET);
	read(fd, &raw, sizeof(raw));
	dataset_raw_to_float(&raw, &ex, &CALIBRATION);
		
	for(int j = dims; j--;)
	{
		ranges[j].min = ex.state.v[j];
		ranges[j].max = ex.state.v[j];
	}

	for(int i = examples; i--;)
	{
		float* st;

		read(fd, &raw, sizeof(raw));
		dataset_raw_to_float(&raw, &ex, &CALIBRATION);
		st = ex.state.v;
		for(int j = dims; j--;)
		{
			float f = st[j];
			if(f > ranges[j].max) ranges[j].max = f;
			if(f < ranges[j].min) ranges[j].min = f;
		}
	}
}


int load_dataset(const char* path)
{
	DS_I_FD = open(path, O_RDONLY);

	if(DS_I_FD < 0)
	{
		EXIT("Failed to open '%s'", path);
	}

	dataset_header_t hdr = {};
	read(DS_I_FD, &hdr, sizeof(hdr));

	if(hdr.magic != MAGIC)
	{
		EXIT("Incompatible version");
	}

	off_t size = lseek(DS_I_FD, 0, SEEK_END);
	DS_SIZE = size / sizeof(raw_example_t);
	lseek(DS_I_FD, 0, SEEK_SET);

	int fd = open("actions.cal", O_RDONLY);
	if(fd >= 0)
	{
		read(fd, &CALIBRATION, sizeof(CALIBRATION));
		close(fd);
	}

	return 0;
}


void proc_opts(int argc, const char ** argv)
{
	for(;;)
	{
		int c = getopt(argc, (char *const *)argv, "i:o:t:s:f");
		if(c == -1) break;

		int min, max;

		switch (c) {
			case 't':
				sscanf(optarg, "%d,%d", &min, &max);
				CALIBRATION.throttle.min = min;
				CALIBRATION.throttle.max = max;
				break;
			case 's':
				sscanf(optarg, "%d,%d", &min, &max);
				CALIBRATION.steering.min = min;
				CALIBRATION.steering.max = max;
				break;
			case 'i':
				load_dataset(optarg);
				DS_PATH = optarg;
				break;
			case 'o':
				DS_O_PATH = optarg;
				DS_O_FD = open(optarg, O_CREAT | O_WRONLY, 0666);
				break;
			case 'f':
				DS_FEATURE_SCALE = 1;
				break;
		}
	}
}


int main(int argc, const char* argv[])
{
	DS_I_FD = 0; // stdin
	DS_O_FD = 1; // stdout

	proc_opts(argc, argv);

	const int dims = sizeof(state_vector_t) / 4;
	example_t examples[DS_SIZE];
	range_t ranges[sizeof(state_vector_t) / 4] = {};
	float mus[sizeof(state_vector_t) / 4] = {};

	//dataset_raw_to_float(RAW_DS, examples, &CALIBRATION, DS_SIZE);
	dataset_range(DS_I_FD, ranges, dims, DS_SIZE);
	dataset_mean(DS_I_FD, mus, dims, DS_SIZE);

	for(int i = dims; i--;)
	{
		printf("%d [%f - %f]\n", i, ranges[i].min, ranges[i].max);
	}

	close(DS_I_FD);
	close(DS_O_FD);

	return 0;
}
