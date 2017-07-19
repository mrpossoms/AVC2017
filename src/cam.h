#ifndef AVC_CAM
#define AVC_CAM

#include <linux/videodev2.h>
#include <inttypes.h>
#include <sys/types.h>

typedef struct {
	int width, height;
	
} cam_settings_t;

typedef struct {
	int fd;
	void* frame_buffer;
	struct v4l2_buffer buffer_info;
} cam_t;

// Set when cam_config is called
extern size_t CAM_BYTES_PER_FRAME;

int   cam_config(int fd, cam_settings_t* cfg);
cam_t cam_open(const char* path, cam_settings_t* cfg);

int cam_request_frame(cam_t* cam);
int cam_wait_frame(cam_t* cam);

#endif
