#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <pthread.h>

#include "dataset_hdr.h"
#include "structs.h"
#include "sys.h"

#include "nn.h"

#define ROOT_DIR "/tmp/"

uint8_t* indexer(mat_t* src, int row, int col, size_t* size)
{
	static const float zeros[256] = {};

	*size = sizeof(float) * src->dims[2];

	// Zero padding for SAME convolutions
	if (row < 0 || col < 0 ||
	    row >= src->dims[0] || col >= src->dims[1])
	{
		return (uint8_t*)zeros;
	}

	int cols = src->dims[1];
	return (uint8_t*)(src->_data.f + (row * cols) + col);
}

mat_value_t sigmoid_f(mat_value_t v)
{
	mat_value_t o = {
		.f = 1 / (1 + powf(M_E, -v.f)),
	};
	return o;
}

mat_value_t relu_f(mat_value_t v)
{
	mat_value_t ret = {
		.f = v.f > 0 ? v.f : 0
	};
	return ret;
}

mat_value_t softmax_num_f(mat_value_t v)
{

	mat_value_t ret = {
		.f = powf(M_E, v.f)
	};
	return ret;
}

#define MAX_POOL_HALF {          \
	.type = POOLING_MAX,         \
	.op = {                      \
	   .stride = { 2, 2 },       \
	   .kernel = { 2, 2 },       \
	   .pixel_indexer = indexer, \
	}                            \
}\

// #define RENDER_DEMO

void yuv422_to_lum8(color_t* yuv, uint8_t* lum8, int w, int h)
{
	for(int i = w * h; i--;)
	{
		lum8[i] = yuv[i].y;
	}
}

float clamp(float v)
{
	v = v > 255 ? 255 : v;
	return  v < 0 ? 0 : v;
}

void yuv422_to_rgb(uint8_t* luma, chroma_t* uv, color_t* rgb, int w, int h)
{
	for(int yi = h; yi--;)
	for(int xi = w; xi--;)
	{
		int i = yi * w + xi;
		int j = yi * (w >> 1) + (xi >> 1);

		rgb[i].r = clamp(luma[i] + 1.14 * (uv[j].cb - 128));
		rgb[i].g = clamp(luma[i] - 0.395 * (uv[j].cr - 128) - (0.581 * (uv[j].cb - 128)));
		rgb[i].b = clamp(luma[i] + 2.033 * (uv[j].cr - 128));
	}
}


void next_example(int fd, raw_example_t* ex)
{
	size_t needed = sizeof(raw_example_t);
	off_t  off = 0;
	uint8_t* buf = (uint8_t*)ex;

	while(needed)
	{
		size_t gotten = read(fd, buf + off, needed);
		needed -= gotten;
		off += gotten;
	}
}


static int oneOK;
int main(int argc, char* argv[])
{
	b_log("Magic %d\n", MAGIC);
	b_log("%dx%d", FRAME_W, FRAME_H);

	color_t rgb[FRAME_W * FRAME_H] = {};
	raw_example_t ex = {};

	int pos_idx = 0;
	int use_sleep = 0;
	int INPUT_FD = 0;

	if(argc >= 2)
	{
		INPUT_FD = open(argv[1], O_RDONLY);
		//use_sleep = 1;
	}

	mat_t x = {
		.type = f32,
		.dims = { 16, 16, 3 },
	};
	nn_mat_init(&x);

	nn_layer_t L[] = {
		{
			.w = nn_mat_load(ROOT_DIR "model/conv2d.kernel"),
			.b = nn_mat_load(ROOT_DIR "model/conv2d.bias"),
			.activation = relu_f,
			.filter = {
				.kernel = { 3, 3 },
				.stride = { 1, 1 },
				.padding = PADDING_SAME,
				.pixel_indexer = indexer
			},
			.pool = MAX_POOL_HALF
		},
		{
			.w = nn_mat_load(ROOT_DIR "model/conv2d_1.kernel"),
			.b = nn_mat_load(ROOT_DIR "model/conv2d_1.bias"),
			.activation = relu_f,
			.filter = {
				.kernel = { 3, 3 },
				.stride = { 1, 1 },
				.padding = PADDING_SAME,
				.pixel_indexer = indexer
			},
			.pool = MAX_POOL_HALF
		},
		{
			.w = nn_mat_load(ROOT_DIR "model/conv2d_2.kernel"),
			.b = nn_mat_load(ROOT_DIR "model/conv2d_2.bias"),
			.activation = relu_f,
			.filter = {
				.kernel = { 3, 3 },
				.stride = { 1, 1 },
				.padding = PADDING_SAME,
				.pixel_indexer = indexer
			},
			.pool = MAX_POOL_HALF
		},
		{
			.w = nn_mat_load(ROOT_DIR "model/dense.kernel"),
			.b = nn_mat_load(ROOT_DIR "model/dense.bias"),
			.activation = relu_f
		},
		{
			.w = nn_mat_load(ROOT_DIR "model/dense_1.kernel"),
			.b = nn_mat_load(ROOT_DIR "model/dense_1.bias"),
			.activation = softmax_num_f
		}
	};

	assert(nn_conv_init(L + 0, &x) == 0);
	for (int i = 1; i < 3; i++)
	{
		assert(nn_conv_init(L + i, (L + i - 1)->A) == 0);
	}

	for (int i = 3; i < 5; i++)
	{
		assert(nn_fc_init(L + i, (L+i-1)->A) == 0);
	}

	dataset_header_t hdr = {};
	int oneOK = read(INPUT_FD, &hdr, sizeof(hdr)) == sizeof(hdr);

	if(hdr.magic != ((uint64_t)MAGIC))
	{
		EXIT("Incompatible version");
	}

	// if (FORWARD_STATE)
	{
		write(1, &hdr, sizeof(hdr));

	}

	mat_t A_1;

	while (1)
	{
		next_example(INPUT_FD, &ex);

		yuv422_to_rgb(ex.state.view.luma, ex.state.view.chroma, rgb, FRAME_W, FRAME_H);

		for (int r = 90; r < 122; r += 16)
		for (int c = 0; c < FRAME_W; c += 16)
		{
			for (int kr = 16; kr--;)
			for (int kc = 16; kc--;)
			{
				color_t color = rgb[((r + kr) * FRAME_W) + c + kc];
				x._data.f[kr * kc * 3 + 0] = color.r / 255.0f - 0.5f;
				x._data.f[kr * kc * 3 + 1] = color.g / 255.0f - 0.5f;
				x._data.f[kr * kc * 3 + 2] = color.b / 255.0f - 0.5f;
			}

			{ // predict
				nn_conv_ff(&x, L + 0);

				for (int i = 1; i < 3; ++i)
				{
					nn_conv_ff(L[i - 1].A, L + i);
				}

				A_1 = *L[2].A;
				A_1.dims[0] = 1;
				A_1.dims[1] = A_1._size;
				A_1.dims[2] = 0;
				A_1._rank = 2;

				for (int i = 3; i < 5; i++)
				{
					nn_fc_ff(L + i, &A_1);
					A_1 = *L[i].A;
					A_1.dims[1] = A_1.dims[0];
					A_1.dims[0] = 1;
				}

				float sum = 0;
				for (int i = A_1._size; i--;) sum += A_1._data.f[i];
				mat_value_t denom = { .f = 1.f / sum };
				nn_mat_scl_e(&A_1, &A_1, denom);

				const int CHROMA_W = FRAME_W / 2;
				for (int kr = 16; kr--;)
				for (int kc = 16; kc--;)
				{
					// ex.state.view.luma[(r + kr) * FRAME_W + (c + kc)] = A_1._data.f[1] * 255;
					if (A_1._data.f[1] > A_1._data.f[2] && A_1._data.f[1] > A_1._data.f[0])
					ex.state.view.chroma[(r + kr) * CHROMA_W + (c + kc) / 2].cb = A_1._data.f[1] * 255;
					// ex.state.view.chroma[(r + kr) * CHROMA_W + (c + kc) / 2].cr = A_1._data.f[2] * 255;
					// ex.state.view.chroma[(r + kr) * FRAME_H + c + kc].cb = A_1._data.f[2] * 255;
				}
			}
		}
		// fprintf(stderr, "%f %f %f\n", A_1._data.f[0], A_1._data.f[1], A_1._data.f[2]);

		write(1, &ex, sizeof(ex));
	}


	return 0;
}
