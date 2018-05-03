#ifndef AVC_NN
#define AVC_NN

#include <sys/types.h>
#include <inttypes.h>

#define NN_MAT_MAX_DIMS 4

typedef enum {
	f32 = 0,
	d64
} mat_storage_t;


struct mat_t;
//typedef float (*mat_initializer)(struct mat_t*);


struct mat_t {
	/**
	 * @brief Type used to represent numeric values in the matrix
	 */
	mat_storage_t type;

	/**
	 * @brief Int array specifying the length of each
	 *        orthoganal dimension. Must be null terminated
	 */
	int dims[NN_MAT_MAX_DIMS];

	/**
	 * @brief Optional: Uses this function to initialize each value
	 *                  of the matrix
	 */
	float (*fill)(struct mat_t*);

	/**
	 * @brief number of dimensions, this will be filled in automatically
	 */
	unsigned int _rank;

	/**
	 * @brief total number of elements
	 */
	unsigned int _size;

	/**
	 * @brief Raw pointer to a contiguous array used
	 *        to store the matrix's data
	 */
	union {
		void* ptr;
		float* f;
		double* d;
	} _data;
};
typedef struct mat_t mat_t;


typedef enum {
	PADDING_VALID,
	PADDING_SAME
} conv_padding_t;


typedef enum {
	POOLING_NONE = 0,
	POOLING_MAX,
} conv_pooling_t;


typedef struct {
	struct {
		int w, h;
	} kernel;

	struct {
		int row, col;
	} stride;

	conv_padding_t padding;

	/**
	 * Returns a pointer to a pixel and all its consecutive channels.
	 *
	 * @param src  Matrix to retrieve a pixel from
	 * @param row  Row the pixel resides in, if outside the bounds of 'src'
	 *             a pointer to a 0 filled buffer of 'size' must be returned.
	 * @param col  Column the pixel resides in, if outside the bounds of 'src'
	 *             a pointer to a 0 filled buffer of 'size' must be returned.
	 * @param size Will contain the size of the pixel and its channels in bytes
	 * @return Pointer to contigious memory containing pixel
	 */
	uint8_t* (*pixel_indexer)(mat_t* src, int row, int col, size_t* size);

	struct {
		int row, col;
	} corner;

} conv_op_t;


typedef struct {
	mat_t w;
	mat_t b;
	float (*activation)(float);

	conv_op_t filter;

	struct {
		conv_op_t op;
		conv_pooling_t type;
		mat_t _PA;
	} pool;

	/**
	 * @brief Final output of activations
	 */
	mat_t* A;

	mat_t _CA;
	mat_t _conv_patch;
	mat_t _z;
} nn_layer_t;


/**
 * @brief Allocates memory for matrix described by 'M'
 * @param M - description of desired matrix
 * @return 0 on success.
 **/
int nn_mat_init(mat_t* M);

// Matrix operations
// ------------------------------------
// All of these functions implicitly succeed, errors or inconsistencies
// will cause program termination

/**
 * @brief Performs matrix multiplication A x B storing the result in R
 * @param R - Resulting matrix of the multiplication. It's dimensions must be valid
 * @param A - Left hand of the muliplication
 * @param B - Right hand of the multiplication
 */
void nn_mat_mul(mat_t* R, mat_t* A, mat_t* B);

void nn_mat_mul_conv(mat_t* R, mat_t* A, mat_t* B);

int nn_mat_max(mat_t* M);

/**
 * @brief Performs element-wise multiplication A x B storing the result in R
 * @param R - Resulting matrix of the multiplication. It's dimensions must be valid
 * @param A - Left hand of the muliplication
 * @param B - Right hand of the multiplication
 */
void nn_mat_mul_e(mat_t* R, mat_t* A, mat_t* B);

/**
 * @brief Performs scaling operation on whole matrix M storing the result in R
 * @param R - Resulting matrix of the multiplication. It's dimensions must be valid
 * @param M - Left hand of the muliplication
 * @param s - Scalar that is multiplied by each element of M
 */
void nn_mat_scl_e(mat_t* R, mat_t* M, float s);

/**
 * @brief Performs element-wise addition A + B storing the result in R
 * @param R - Resulting matrix of the addition. It's dimensions must be valid
 * @param A - Left hand of the addition
 * @param B - Right hand of the addition
 */
void nn_mat_add_e(mat_t* R, mat_t* A, mat_t* B);

/**
 * Applies function element wise to all values in matrix M.
 * @param R    Result of func on M. R must be the same shape as M.
 * @param M    Matrix whose values will be passed through func
 * @param func Pointer to a function that takes a numeric value, apply
 *             some transformation and returns the result.
 */
void nn_mat_f(mat_t* R, mat_t* M, float (*func)(float));

mat_t nn_mat_load(const char* path);

int nn_fc_init(nn_layer_t* li, mat_t* a_in);

void nn_fc_ff(nn_layer_t* li, mat_t* a_in);

int nn_conv_init(nn_layer_t* li, mat_t* a_in);

// mat_t nn_conv_filter(mat_t* W, int filter_index);

void nn_conv_patch(mat_t* patch, mat_t* src, conv_op_t op);

void nn_conv_max_pool(mat_t* pool, mat_t* src, conv_op_t op);

void nn_conv_ff(mat_t* a_in, nn_layer_t* li);

#endif
