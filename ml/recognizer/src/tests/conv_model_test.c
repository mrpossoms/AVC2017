#include "test.h"
#include "../nn.h"
#include "../nn.c"


uint8_t* indexer(mat_t* src, int row, int col, size_t* size)
{
	static const float zeros[256] = {};

	*size = sizeof(float) * src->dims[2];

	// Zero padding for SAME convolutions
	if (row < 0 || col < 0 ||
	    row >= src->dims[0] || col >= src->dims[1])
	{
		return zeros;
	}

	int cols = src->dims[1];
	return (uint8_t*)(src->_data.f + (row * cols) + col);
}


mat_value_t relu_f(mat_value_t v)
{
	mat_value_t ret = {
		.f = v.f > 0 ? v.f : 0
	};
	return ret;
}


int model_test(void)
{
	mat_t x = {
		.type = f32,
		.dims = { 32, 32, 3 }
	};
	nn_mat_init(&x);

	nn_layer_t L[] = {
		{
			.w = nn_mat_load("model/conv2d.kernel"),
			.b = nn_mat_load("model/conv2d.bias"),
			.activation = relu_f
		},
		{
			.w = nn_mat_load("model/conv2d_1.kernel"),
			.b = nn_mat_load("model/conv2d_1.bias"),
			.activation = relu_f
		},
		{
			.w = nn_mat_load("model/conv2d_2.kernel"),
			.b = nn_mat_load("model/conv2d_2.bias"),
			.activation = relu_f
		},

	};
	for (int i = 3; i--;)
	{
		nn_conv_init(L + i);
	}

	mat_t X = {
		.dims = { 32, 32, 3 }
	};
	nn_mat_init(&X);

	mat_t A[] = {
		{ .dims = { 32, 32,  32 } },
		{ .dims = { 16, 16,  64 } },
		{ .dims = {  8,  8, 128 } },
	};

	mat_t P[] = {
		{ .dims = { 16, 16,  32 } },
		{ .dims = {  8,  8,  64 } },
		{ .dims = {  4,  4, 128 } },
	};

	for (int i = 3; i--;)
	{
		nn_mat_init(A + i);
		nn_mat_init(P + i);
	}

	conv_op_t op = {
		.stride = { 1, 1 },
		.kernel = { 3, 3 },
		.padding = PADDING_SAME,
		.pixel_indexer = indexer
	};

	nn_conv(&X, A + 0, L + 0, op);

	for (int i = 1; i < 3; ++i)
	{
		// conv[i]
	}

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
