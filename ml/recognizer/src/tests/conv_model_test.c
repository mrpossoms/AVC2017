#include "test.h"
#include "../nn.h"
#include "../nn.c"
#include "hay.h"

#define ROOT_DIR "/tmp/"

float hay[] = HAY;

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

#define MAX_POOL_HALF {          \
	.type = POOLING_MAX,         \
	.op = {                      \
	   .stride = { 2, 2 },       \
	   .kernel = { 2, 2 },       \
	   .pixel_indexer = indexer, \
	}                            \
}\

int model_test(void)
{
	mat_t x = {
		.type = f32,
		.dims = { 32, 32, 3 },
		._data.f = hay
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
			.activation = sigmoid_f
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

	nn_conv_ff(&x, L + 0);

	for (int i = 1; i < 3; ++i)
	{
		nn_conv_ff(L[i - 1].A, L + i);
	}

	mat_t A_1 = *L[2].A;
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

	Log("%f %f %f", 1,
	A_1._data.f[0],
	A_1._data.f[1],
	A_1._data.f[2]);

	// mat_t fcw0 = nn_mat_load("model/dense.kernel");
	// mat_t fcb0 = nn_mat_load("model/dense.bias");
	// mat_t fcw1 = nn_mat_load("model/dense_1.kernel");
	// mat_t fcb1 = nn_mat_load("model/dense_1.bias");

	return 0;
}

TEST_BEGIN
	.name = "conv_model_test",
	.description = "Puts everything together and builds a convolutional model",
	.run = model_test,
TEST_END
