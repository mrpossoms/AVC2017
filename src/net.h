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

float sigmoid_f(float v)
{
	return 1 / (1 + powf(M_E, -v));
}

float relu_f(float v)
{
	return v > 0 ? v : 0;
}

float softmax_num_f(float v)
{
	return powf(M_E, v);
}
