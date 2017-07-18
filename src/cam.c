#include <sys/ioctl.h>

#include "cam.h"

int cam_open(const char* path, cam_settings_t* cfg)
{
	int fd = open(path, O_RDONLY);

	if(fd < 0)
	{
		return -1;
	}

	if(cam_config(cfg) < 0)
	{
		return -2;
	}

	return fd;
}

int cam_config(cam_settings_t* cfg)
{
	

	return 0;
}
