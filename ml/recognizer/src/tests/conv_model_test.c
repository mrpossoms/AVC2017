#include "test.h"
#include "../nn.h"
#include "../nn.c"

int model_test(void)
{
	mat_t x = {
		.type = f32,
		.dims = { 32, 32, 3 }
	};
	nn_mat_init(&x);

	mat_t w0 = nn_mat_load("model/conv2d.kernel");
	mat_t b0 = nn_mat_load("model/conv2d.bias");
	mat_t w1 = nn_mat_load("model/conv2d_1.kernel");
	mat_t b1 = nn_mat_load("model/conv2d_1.bias");
	mat_t w2 = nn_mat_load("model/conv2d_2.kernel");
	mat_t b2 = nn_mat_load("model/conv2d_2.bias");

	mat_t fcw0 = nn_mat_load("model/dense.kernel");
	mat_t fcb0 = nn_mat_load("model/dense.bias");
	mat_t fcw1 = nn_mat_load("model/dense_1.kernel");
	mat_t fcb1 = nn_mat_load("model/dense_1.bias");

	return 0;
}

TEST_BEGIN
	.name = "conv_model_test",
	.description = "Puts everything together and builds a convolutional model",
	.run = model_test,
TEST_END
