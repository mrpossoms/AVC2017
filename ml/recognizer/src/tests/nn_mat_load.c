#include "test.h"
#include "../nn.h"
#include "../nn.c"

int mat_load(void)
{
	mat_t M = nn_mat_load("model/conv2d.bias");

	assert(M._data.ptr);
	assert(M.dims[0] == 16);
	assert(M.dims[1] == 1);
	assert(M._size == 16);
	assert(M._rank == 2);

	return 0;
}

TEST_BEGIN
	.name = "nn_mat_load",
	.description = "Checks matrix loading.",
	.run = mat_load,
TEST_END
