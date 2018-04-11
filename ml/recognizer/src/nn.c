#include "nn.h"
#include <assert.h>
#include <stdlib.h>

static mat_value_t zero_fill(mat_t* M)
{
	const mat_value_t zero = {};
	return zero;
}

static size_t value_size(mat_t* M)
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
	for (M->_size = 0; M->_size < NN_MAT_MAX_DIMS && M->dims[M->_size];)
	{
		++M->_size;
	}

	// size less than 2 is no good
	assert(M->_size >= 2);

	size_t total_elements = M->dims[0];
	for (int i = 1; i < M->_size; ++i)
	{
		total_elements *= M->dims[i];
	}

	M->_data = calloc(total_elements, value_size(M));

	// Check for allocation failure
	if (!M->_data) return -2;

	// perform fill initialization
	for (int i = total_elements; i--;)
	{
		switch(M->type)
		{
			case f32:
				((float*)M->_data)[i] = M->fill(M).f;
				break;
			case d64:
				((double*)M->_data)[i] = M->fill(M).d;
				break;
		}
	}

	return 0;
}

#define e2f(M, i, j) (((float*)(M)->_data) + (M)->dims[1] * i + j)
#define e2d(M, i, j) (((double*)(M)->_data) + (M)->dims[1] * i + j)

void nn_mat_mul(mat_t* R, mat_t* A, mat_t* B)
{
	// MxN * NxO = MxO
	
	assert(R._size == A._size);
	assert(A._size == B._size);
	assert(A.dims[1] == B.dims[0]);

	for (int ar = A.dims[0]; ar--;)
	for (int bc = B.dims[1]; bc--;)
	{
		float dot = 0;
		for (int i = B.dims[0]; i--;)
		{
			dot += e2f(A, ar, i) * e2f(B, i, bc);
		}

		e2f(R, ar, bc) = dot;
	}
}


void nn_mat_mul_e(mat_t* R, mat_t* A, mat_t* B)
{
	assert(R._size == A._size);
	assert(A._size == B._size);
	assert(A.dims[0] == B.dims[0] && A.dims[1] == B.dims[1]);

	for (int r = A.dims[0]; r--;)
	for (int c = A.dims[1]; c--;)
	{
		e2f(R, r, c) = e2f(A, r, c) * e2f(B, r, c);
	}
}

void nn_mat_add_e(mat_t* R, mat_t* A, mat_t* B)
{
	assert(R._size == A._size);
	assert(A._size == B._size);
	assert(A.dims[0] == B.dims[0] && A.dims[1] == B.dims[1]);

	for (int r = A.dims[0]; r--;)
	for (int c = A.dims[1]; c--;)
	{
		e2f(R, r, c) = e2f(A, r, c) * e2f(B, r, c);
	}
}

void nn_mat_scl_e(mat_t* R, mat_t* M, mat_value_t s)
{
	assert(R._size == M._size);
	assert(M._size == R._size);
	assert(R.dims[0] == M.dims[0] && R.dims[1] == M.dims[1]);

	for (int r = M.dims[0]; r--;)
	for (int c = M.dims[1]; c--;)
	{
		e2f(R, r, c) = e2f(M, r, c) * s.f;
	}
}
