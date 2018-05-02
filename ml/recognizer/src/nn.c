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

	if (M->_data.ptr == NULL)
	{
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
	}

	return 0;
}


void nn_mat_mul(mat_t* R, mat_t* A, mat_t* B)
{
	// MxN * NxO = MxO

	assert(R->_rank == A->_rank);
	assert(A->_rank == B->_rank);
	if(A->dims[0] != B->dims[1])
	{
		fprintf(stderr,
		        "nn_mat_mul: %dx%d not compatible with %dx%d\n",
		        A->dims[0], A->dims[1],
		        B->dims[0], B->dims[1]);

		exit(-1);
	}

	for (int ar = A->dims[0]; ar--;)
	for (int bc = B->dims[1]; bc--;)
	{
		volatile float dot = 0;
		int i = B->dims[0];
		while (i)
		{
			if (i > 32)
			{
				float d __attribute__ ((vector_size (4)));
				float a __attribute__ ((vector_size (4)));
				float b __attribute__ ((vector_size (4)));

				// for (int j = 32; j--;)
				d[0] = a[0] * b[0];
				d[1] = a[1] * b[1];
				d[2] = a[2] * b[2];
				d[3] = a[3] * b[3];

				i -= 32;
			}
			else
			{
				i -= 1;
				dot += e2f(A, ar, i) * e2f(B, i, bc);
			}
		}

		// for (int i = B->dims[0]; i--;)
		// {
		// 	dot += e2f(A, ar, i) * e2f(B, i, bc);
		// }
		//
		// e2f(R, ar, bc) = dot;
	}
}


void nn_mat_mul_e(mat_t* R, mat_t* A, mat_t* B)
{
	assert(R->_rank == A->_rank);
	assert(A->_rank == B->_rank);
	if(!(A->dims[0] == B->dims[0] && A->dims[1] == B->dims[1]))
	{
		fprintf(stderr,
		        "nn_mat_mul_e: %dx%d not compatible with %dx%d\n",
		        A->dims[0], A->dims[1],
		        B->dims[0], B->dims[1]);

		exit(-1);
	}


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
	if(!(A->dims[0] == B->dims[0] && A->dims[1] == B->dims[1]))
	{
		fprintf(stderr,
		        "nn_mat_add_e: %dx%d not compatible with %dx%d\n",
		        A->dims[0], A->dims[1],
		        B->dims[0], B->dims[1]);

		exit(-1);
	}

	for (int r = A->dims[0]; r--;)
	for (int c = A->dims[1]; c--;)
	{
		e2f(R, r, c) = e2f(A, r, c) + e2f(B, r, c);
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


mat_t nn_mat_reshape(mat_t* M, ...)
{
	mat_t R = *M;
	va_list args;

	memset(R.dims, 0, sizeof(int) * NN_MAT_MAX_DIMS);
	va_start(args, M);

	int i = 0;
	for (int d = va_arg(args, int); d;)
	{
		R.dims[i++] = d;
	}

	va_end(args);

	return R;
}


int nn_conv_init(nn_layer_t* li, mat_t* a_in)
{
	int res = 0;
	assert(li);
	assert(a_in);

	int a_rows = a_in->dims[0];
	int a_cols = a_in->dims[1];
	int depth_in = li->w.dims[2];
	int depth_out = li->w.dims[3];

	{ // Setup matrices for weights and biases
		li->w.dims[1] = li->w.dims[0] * li->w.dims[1] * depth_in;
		li->w.dims[0] = depth_out;
		li->w.dims[2] = li->w.dims[3] = 0;
		res += nn_mat_init(&li->w) * -10;

		if (res) return res;

		mat_t b = {
			.type = f32,
			.dims = { depth_out, 1 }
		};
		res += nn_mat_init(&b) * -20;
		li->b = b;

		if (res) return res;
	}

	{ // Setup preactivation vector
		mat_t z = {
			.type = f32,
			.dims = { depth_out, 1 }
		};
		res += nn_mat_init(&z) * -30;
		li->_z = z;

		if (res) return res;
	}

	{ // Setup patch vector
		mat_t patch = {
			.type = f32,
			.dims = { li->w.dims[1], 1 }
		};
		res += nn_mat_init(&patch) * -40;
		li->_conv_patch = patch;

		if (res) return res;
	}

	{ // Setup convolution activation map
		int pad_row = 0;
		int pad_col = 0;

		conv_op_t f = li->filter;
		if (f.padding == PADDING_SAME)
		{
			pad_row = f.kernel.h / 2;
			pad_col = f.kernel.w / 2;
		}

		int ca_rows = ((a_rows - f.kernel.h + 2 * pad_row) / f.stride.row) + 1;
		int ca_cols = ((a_cols - f.kernel.w + 2 * pad_col) / f.stride.col) + 1;

		mat_t CA = {
			.type = f32,
			.dims = { ca_rows, ca_cols, depth_out }
		};
		res += nn_mat_init(&CA) * -50;
		li->_CA = CA;
		li->A = &li->_CA;

		if (res) return res;
	}

	// Setup pooling matrix
	switch (li->pool.type)
	{
		case POOLING_MAX:
		{
			mat_t PA = {
				.type = f32,
				.dims = {
					li->_CA.dims[0] / li->pool.op.kernel.h,
					li->_CA.dims[1] / li->pool.op.kernel.w,
					depth_out
				}
			};
			res += nn_mat_init(&PA) * -60;
			li->pool._PA = PA;
			li->A = &li->pool._PA;
			if (res) return res;
		}
		case POOLING_NONE:;
	}

	return res;
}


void nn_conv_patch(mat_t* patch, mat_t* src, conv_op_t op)
{
	assert(patch->_data.ptr);
	assert(src->_data.ptr);

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


void nn_conv(mat_t* a_in, nn_layer_t* li)
{
	assert(a_in);
	assert(li);
	// assert(a_in->_rank == 3);
	// assert(a_out->_rank == 3);

	conv_op_t op = li->filter;
	int pad_row = 0;
	int pad_col = 0;
	mat_t* patch = &li->_conv_patch;

	if (op.padding == PADDING_SAME)
	{
		pad_row = op.kernel.h / 2;
		pad_col = op.kernel.w / 2;
	}

	// For each pile of channels in the pool...
	for (int p_row = li->_CA.dims[0]; p_row--;)
	for (int p_col = li->_CA.dims[1]; p_col--;)
	{
		op.corner.row = p_row * op.stride.row - pad_row;
		op.corner.col = p_col * op.stride.col - pad_col;

		// get the convolution window from the input activation volume
		nn_conv_patch(patch, a_in, op);

		// apply the filter
		nn_mat_mul(&li->_z, patch, &li->w);
		nn_mat_add_e(&li->_z, &li->_z, &li->b);

		size_t feature_depth;
		float* z_pile = (float*)op.pixel_indexer(&li->_CA, p_row, p_col, &feature_depth);

		memcpy(z_pile, li->_z._data.f, feature_depth);
	}

	// activate
	nn_mat_f(&li->_CA, &li->_CA, li->activation);

	// Apply pooling if specified
	switch (li->pool.type)
	{
		case POOLING_MAX:
		{
			nn_conv_max_pool(&li->pool._PA, &li->_CA, li->pool.op);
		}
		case POOLING_NONE:;
	}
}


void nn_conv_max_pool(mat_t* pool, mat_t* src, conv_op_t op)
{
	int exp_size[NN_MAT_MAX_DIMS] = {
		src->dims[0] / op.stride.row,
		src->dims[1] / op.stride.col
	};

	assert(pool->_rank == src->_rank);
	for (int i = 2; i--;) assert(exp_size[i] == pool->dims[i]);

	// For each pile of channels in the pool...
	for (int p_row = pool->dims[0]; p_row--;)
	for (int p_col = pool->dims[1]; p_col--;)
	{
		// get the pile address
		size_t size;
		float* pool_pile = (float*)op.pixel_indexer(
			pool, p_row, p_col, &size);

		// For each kernel window position
		for (int k_row = op.kernel.w; k_row--;)
		for (int k_col = op.kernel.h; k_col--;)
		{
			int s_row = p_row * op.stride.row + k_row;
			int s_col = p_col * op.stride.col + k_col;

			float* src_pile = (float*)op.pixel_indexer(
				src, s_row, s_col, &size);

			for (int chan = pool->dims[2]; chan--;)
			{
				if (pool_pile[chan] < src_pile[chan])
				{
					pool_pile[chan] = src_pile[chan];
				}
			}
		}
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
