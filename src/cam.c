#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include "structs.h"
#include "cam.h"

cam_t cam_open(const char* path, cam_settings_t* cfg)
{

	int fd = open(path, O_RDWR);

	if(fd < 0)
	{
		fprintf(stderr, "Error opening video device '%s'\n", path);
		exit(-1);
	}

	struct v4l2_capability cap;
	if(ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0)
	{
		fprintf(stderr, "Error querying '%s' for capabilities\n", path);
		exit(-2);
	}

	if(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
	{
		fprintf(stderr, "Error, '%s' lacks V4L2_CAP_VIDEO_CAPTURE capability\n", path);
	}

	if(cam_config(fd, cfg) < 0)
	{
		fprintf(stderr, "Error configuring '%s'\n", path);
		exit(-3);
	}

	// Inform v4l about the buffers we want to receive data through
	struct v4l2_requestbuffers bufrequest;
	bufrequest.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	bufrequest.memory = V4L2_MEMORY_MMAP;
	bufrequest.count = 1;

	if(ioctl(fd, VIDIOC_REQBUFS, &bufrequest) < 0)
	{
		fprintf(stderr, "VIDIOC_REQBUFS\n");
		exit(-4);
	}


	// Find the buffer size
	struct v4l2_buffer bufferinfo;
	memset(&bufferinfo, 0, sizeof(bufferinfo));

	bufferinfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	bufferinfo.memory = V4L2_MEMORY_MMAP;
	bufferinfo.index = 0;

	if(ioctl(fd, VIDIOC_QUERYBUF, &bufferinfo) < 0)
	{
		fprintf(stderr, "VIDIOC_QUERYBUF\n");
		exit(-5);
	}

	void* frame_buffer = mmap(
		NULL,
		bufferinfo.length,
		PROT_READ | PROT_WRITE,
		MAP_SHARED,
		fd,
		bufferinfo.m.offset
	);	

	bzero(frame_buffer, bufferinfo.length);

	if(frame_buffer == MAP_FAILED)
	{
		fprintf(stderr, "mmap failed\n");
		exit(-6);
	}

	cam_t cam = {
		.fd = fd,
		.frame_buffer = frame_buffer,
		.buffer_info = bufferinfo,
	};

	cam_wait_frame(&cam);

	int type = bufferinfo.type;
	if(ioctl(fd, VIDIOC_STREAMON, &type) < 0)
	{
		fprintf(stderr, "Error starting streaming\n");
		exit(-7);
	}

	return cam;
}

int cam_request_frame(cam_t* cam)
{
	cam->buffer_info.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	cam->buffer_info.memory = V4L2_MEMORY_MMAP;

	return ioctl(cam->fd, VIDIOC_QBUF, &cam->buffer_info);
}

int cam_wait_frame(cam_t* cam)
{
	return ioctl(cam->fd, VIDIOC_DQBUF, &cam->buffer_info);
}

int cam_config(int fd, cam_settings_t* cfg)
{
	struct v4l2_format format;

	if(!cfg)
	{
		fprintf(stderr, "Error, null configuration provided\n");
		return -1;
	}

/*
	if(ioctl(fd, VIDIOC_G_FMT, &format) < 0)
	{
		fprintf(stderr, "Error, failed retrieving camera settings\n");
		return -2;
	}
*/

	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	format.fmt.pix.width = cfg->width;
	format.fmt.pix.height = cfg->height;

	if(ioctl(fd, VIDIOC_S_FMT, &format) < 0)
	{
		fprintf(stderr, "Error, failed applying camera settings\n");
		return -3;
	}

	struct v4l2_streamparm parm;

	parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	parm.parm.capture.timeperframe.numerator = 30;
	parm.parm.capture.timeperframe.denominator = 1;

	int ret = ioctl(fd, VIDIOC_S_PARM, &parm);

	return 0;
}
