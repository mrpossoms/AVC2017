#ifndef AVC_NN
#define AVC_NN

#define NN_MAT_MAX_DIMS 4

typedef enum {
	f32 = 0,
	d64
} mat_storage_t;

typedef union {
	float f;
	double d;
} mat_value_t;

struct mat_t;
//typedef mat_value_t (*mat_initializer)(struct mat_t*);

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
	mat_value_t (*fill)(struct mat_t*);

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
void nn_mat_scl_e(mat_t* R, mat_t* M, mat_value_t s);

/**
 * @brief Performs element-wise addition A + B storing the result in R
 * @param R - Resulting matrix of the addition. It's dimensions must be valid
 * @param A - Left hand of the addition
 * @param B - Right hand of the addition
 */
void nn_mat_add_e(mat_t* R, mat_t* A, mat_t* B);

void nn_mat_f(mat_t* R, mat_t* M, mat_value_t (*func)(mat_value_t));

#endif
