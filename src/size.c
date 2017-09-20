#include "structs.h"
#include <stdio.h>

int main(void)
{
	raw_example_t ex;
	fprintf(stderr, "%lu %lu\n", sizeof(ex.state.view.luma), sizeof(ex.state.view.chroma));
	printf("%lu\n", sizeof(ex));
	return 0;
}
