#include "nn.h"
#include <assert.h>
#include <stdlib.h>

#define e2f(M, i, j) ((M)->_data.f[(M)->dims[1] * i + j])
#define e2d(M, i, j) ((M)->_data.d[(M)->dims[1] * i + j])


static mat_value_t zero_fill(mat_t* M)
{
	const mat_value_t zero = {};
	return zero;
}


static size_t value_rank(mat_t* M)
{
	switch(M->type)
	{
		case d64:
			return sizeof(double);
		case f32:
		default:
			return sizeof(float);
	}
}


int nn_mat_init(mat_t* M)
{
	if (!M) return -1;
	if (!M->fill)
	{
		M->fill = zero_fill;
	}

	// determine the dimensionality asked for
	M->_size = 1;
	for (M->_rank = 0; M->_rank < NN_MAT_MAX_DIMS && M->dims[M->_rank];)
	{
		M->_size *= M->dims[M->_rank];
		++M->_rank;
	}

	// size less than 2 is no good
	assert(M->_rank >= 2);

	size_t total_elements = M->dims[0];
	for (int i = 1; i < M->_rank; ++i)
	{
		total_elements *= M->dims[i];
	}

	M->_data.ptr = calloc(total_elements, value_rank(M));

	// Check for allocation failure
	if (!M->_data.ptr) return -2;

	// perform fill initialization
	for (int i = total_elements; i--;)
	{
		switch(M->type)
		{
			case f32:
				(M->_data.f)[i] = M->fill(M).f;
				break;
			case d64:
				(M->_data.d)[i] = M->fill(M).d;
				break;
		}
	}

	return 0;
}


void nn_mat_mul(mat_t* R, mat_t* A, mat_t* B)
{
	// MxN * NxO = MxO

	assert(R->_rank == A->_rank);
	assert(A->_rank == B->_rank);
	assert(A->dims[1] == B->dims[0]);



	for (int ar = A->dims[0]; ar--;)
	for (int bc = B->dims[1]; bc--;)
	{
		float dot = 0;
		for (int i = B->dims[0]; i--;)
		{
			dot += e2f(A, ar, i) * e2f(B, i, bc);
		}

		e2f(R, ar, bc) = dot;
	}
}


void nn_mat_mul_e(mat_t* R, mat_t* A, mat_t* B)
{
	assert(R->_rank == A->_rank);
	assert(A->_rank == B->_rank);
	assert(A->dims[0] == B->dims[0] && A->dims[1] == B->dims[1]);



	for (int r = A->dims[0]; r--;)
	for (int c = A->dims[1]; c--;)
	{
		e2f(R, r, c) = e2f(A, r, c) * e2f(B, r, c);
	}
}


void nn_mat_add_e(mat_t* R, mat_t* A, mat_t* B)
{
	assert(R->_rank == A->_rank);
	assert(A->_rank == B->_rank);
	assert(A->dims[0] == B->dims[0] && A->dims[1] == B->dims[1]);

	for (int r = A->dims[0]; r--;)
	for (int c = A->dims[1]; c--;)
	{
		e2f(R, r, c) = e2f(A, r, c) * e2f(B, r, c);
	}
}


void nn_mat_scl_e(mat_t* R, mat_t* M, mat_value_t s)
{
	assert(R->_rank == M->_rank);
	assert(M->_rank == R->_rank);
	assert(R->dims[0] == M->dims[0] && R->dims[1] == M->dims[1]);

	for (int r = M->dims[0]; r--;)
	for (int c = M->dims[1]; c--;)
	{
		e2f(R, r, c) = e2f(M, r, c) * s.f;
	}
}


void nn_mat_f(mat_t* R, mat_t* M, mat_value_t (*func)(mat_value_t))
{
	assert(R->_size == M->_size);

	switch (R->type) {
		case f32:
		for (int i = R->_size; i--;)
		{
			R->_data.f[i] = func((mat_value_t)M->_data.f[i]).f;
		}
		break;
		case d64:
		for (int i = R->_size; i--;)
		{
			R->_data.d[i] = func((mat_value_t)M->_data.d[i]).d;
		}
		break;
	}
}


void nn_conv_patch(mat_t* patch, mat_t* src, conv_op_t op)
{
	for (int row = op.kernel.h; row--;)
	for (int col = op.kernel.w; col--;)
	{
		int ri = op.corner.row + row;
		int ci = op.corner.col + col;
		int i = row * op.kernel.w + col;
		size_t pix_size;
		uint8_t* pixel_chan = op.pixel_indexer(src,
		                                       ri,
		                                       ci,
		                                       &pix_size);

		uint8_t* patch_bytes = (uint8_t*)patch->_data.ptr;
		memcpy(patch_bytes + (pix_size * i), pixel_chan, pix_size);
	}
}


mat_t nn_mat_load(const char* path)
{
	mat_t M = { .type = f32 };
	uint8_t dims = 0;
	int fd = open(path, O_RDONLY);

	// open file, read the dimensions
	if (fd < 0) goto abort;
	if (read(fd, &dims, sizeof(uint8_t)) != sizeof(uint8_t)) goto abort;
	for (int i = 0; i < dims; ++i)
	{
		if (read(fd, M.dims + i, sizeof(int)) != sizeof(int)) goto abort;
	}

	if (dims == 1)
	{
		M.dims[1] = 1;
	}

	// allocate space for the matrix
	if (nn_mat_init(&M)) goto abort;

	// read the entire matrix
	size_t M_size = sizeof(float) * M._size;
	if (read(fd, M._data.ptr, M_size) != M_size) goto abort;

	close(fd);

	return M;
abort:
	free(M._data.ptr);
	M._data.ptr = NULL;
	close(fd);
	return M;
}
