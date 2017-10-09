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
#include <errno.h>
#include <assert.h>

#include "structs.h"
#include "cam.h"

#define CAM_FPS 5

cam_t cam_open(const char* path, cam_settings_t* cfg)
{

	int fd = open(path, O_RDWR);
	int res;

	if(fd < 0)
	{
		fprintf(stderr, "Error opening video device '%s'\n", path);
		//exit(-1);

		cam_t empty = {};
		return empty;
	}

	struct v4l2_capability cap;
	res = ioctl(fd, VIDIOC_QUERYCAP, &cap);
	if(res < 0)
	{
		fprintf(stderr, "Error: %d querying '%s' for capabilities (%d)\n", res, path, errno);
		exit(-2);
	}

	if(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
	{
		fprintf(stderr, "Error: '%s' lacks V4L2_CAP_VIDEO_CAPTURE capability\n", path);
	}

	res = cam_config(fd, cfg);
	if(res < 0)
	{
		fprintf(stderr, "Error: %d configuring '%s' (%d)\n", res, path, errno);
		exit(-3);
	}

	// Inform v4l about the buffers we want to receive data through
	struct v4l2_requestbuffers bufrequest = {};
	bufrequest.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	bufrequest.memory = V4L2_MEMORY_MMAP;
	bufrequest.count = 2;

	if(ioctl(fd, VIDIOC_REQBUFS, &bufrequest) < 0)
	{
		fprintf(stderr, "VIDIOC_REQBUFS\n");
		exit(-4);
	}


	if(bufrequest.count < 2)
	{
		fprintf(stderr, "Not enough memory\n");
		exit(-5);
	}


	struct v4l2_buffer bufferinfo = {};
	void** fbs = calloc(sizeof(void*), bufrequest.count);
	for(int i = bufrequest.count; i--;)
	{
		bufferinfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		bufferinfo.memory = V4L2_MEMORY_MMAP;
		bufferinfo.index = i;

		if(ioctl(fd, VIDIOC_QUERYBUF, &bufferinfo) < 0)
		{
			fprintf(stderr, "VIDIOC_QUERYBUF\n");
			exit(-5);
		}

		fbs[i] = mmap(
			NULL,
			bufferinfo.length,
			PROT_READ | PROT_WRITE,
			MAP_SHARED,
			fd,
			bufferinfo.m.offset
		);

		if(fbs[i] == MAP_FAILED)
		{
			fprintf(stderr, "mmap failed\n");
			exit(-6);
		}

		bzero(fbs[i], bufferinfo.length);
		ioctl(fd, VIDIOC_QBUF, &bufferinfo);
	}


	cam_t cam = {
		.fd = fd,
		.frame_buffers = fbs,
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
	ioctl(cam->fd, VIDIOC_DQBUF, &cam->buffer_info);
}


int cam_config(int fd, cam_settings_t* cfg)
{
	int res = 0;
	struct v4l2_format format;

	if(!cfg)
	{
		fprintf(stderr, "Error: null configuration provided\n");
		return -1;
	}

/*
	res = ioctl(fd, VIDIOC_G_FMT, &format);
	if(res < 0)
	{
		fprintf(stderr, "Error: failed retrieving camera settings (%d)\n", errno);
		return -2;
	}
*/

	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	format.fmt.pix.width = cfg->width;
	format.fmt.pix.height = cfg->height;

	if(ioctl(fd, VIDIOC_S_FMT, &format) < 0)
	{
		fprintf(stderr, "Error: failed applying camera settings\n");
		return -3;
	}

	struct v4l2_streamparm parm = {};

	parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	parm.parm.capture.timeperframe.numerator = 1;
	parm.parm.capture.timeperframe.denominator = cfg->frame_rate;

	res = ioctl(fd, VIDIOC_S_PARM, &parm);

	return 0;
}
