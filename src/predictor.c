#include "sys.h"

int INPUT_FD = 0;

void proc_opts(int argc, const char ** argv)
{
	
}


int main(int argc, const char* argv[])
{
	proc_opts(argc, argv);

	while(1)
	{
		int ret = 0;
		fd_set fds;
		struct timeval tv = { .tv_usec = 1000 * 100 };
		raw_example_t ex = {};

		FD_ZERO(&fds);
		FD_SET(INPUT_FD, &fds);

		ret = select(1, &fds, NULL, NULL, &tv);

		if(ret == -1) // error
		{
			// TODO
		}
		else if(ret) // stuff to read
		{

		}
		else // timeout
		{
			break;
		}

		read(INPUT_FD, &ex, sizeof(ex));
	}

	return 0;
}
