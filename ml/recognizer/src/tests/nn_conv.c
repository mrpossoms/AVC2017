#include "test.h"
#include "../nn.h"
#include "../nn.c"

void* indexer(mat_t* src, int row, int col, size_t* size)
{
	int cols = src->dims[1];
	*size = sizeof(float);
	return (void*)(src->_data.f + (row * cols) + col);
}

mat_value_t sigmoid_f(mat_value_t v)
{
	mat_value_t o = {
		.f = 1 / (1 + powf(M_E, -v.f)),
	};
	return o;
}


int conv_patch(void)
{
	float x0_s[] = {
		0, 1, 1,
		0, 1, 1,
		0, 1, 1
	};
	mat_t X0 = {
		.type = f32,
		.dims = { 3, 3 },
		._rank = 3,
		._size = 9,
		._data.f = x0_s
	};

	float x1_s[] = {
		1, 1, 1,
		1, 1, 1,
		1, 1, 1
	};
	mat_t X1 = {
		.type = f32,
		.dims = { 3, 3 },
		._rank = 3,
		._size = 9,
		._data.f = x1_s
	};


	mat_t A = {
		.type = f32,
		.dims = { 1, 1, 1 }
	};
	nn_mat_init(&A);


	float w_s[] = {
		-1, 0, 1,
		-1, 0, 1,
		-1, 0, 1,
	};
	float b_s[] = {
		0
	};
	nn_layer_t conv = {
		.w = {
			.type = f32,
			.dims = { 3, 3, 1, 1 },
			._data.f = w_s
		},
		.activation = sigmoid_f
	};
	assert(nn_conv_init(&conv) == 0);

	conv_op_t op = {
		.kernel = { 3, 3 },
		.stride = { 1, 1 },
		.pixel_indexer = indexer
	};

	nn_conv(&X0, &A, &conv, op);
	Log("A[0] -> %f\n", 1, A._data.f[0]);
	assert(A._data.f[0] > 0.5);

	nn_conv(&X1, &A, &conv, op);
	Log("A[0] -> %f\n", 1, A._data.f[0]);
	assert(A._data.f[0] <= 0.5);

	return 0;
}

TEST_BEGIN
	.name = "nn_conv",
	.description = "Runs one convolution.",
	.run = conv_patch,
TEST_END
