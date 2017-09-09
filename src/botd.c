#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>

static const char* MEDIA_PATH;

void proc_opts(int argc, const char* argv[])
{
	while(1)
	{
		int c;
	       	while ((c = getopt(argc, argv, "m:")) != -1)
		{
			switch(c)
			{
				case 'm':
					MEDIA_PATH = optarg;
					break;
			}
		}
	}
}


int main(int argc, const char* argv[])
{
	proc_opts(argc, argv);
	
	if(!MEDIA_PATH) return -1;

	

	pid_t child_pid = fork();

	if(child_pid < 0) return -2;
	if(child_pid == 0)
	{
		// child
		
	}


	return 0;
}
