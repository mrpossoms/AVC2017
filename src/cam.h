#ifndef AVC_CAM
#define AVC_CAM

#include <inttypes.h>
#include <sys/types.h>

typedef struct {
	// TODO
} cam_settings_t;

// Set when cam_config is called
extern size_t CAM_BYTES_PER_FRAME;

int cam_config(cam_settings_t* cfg);
int cam_open(const char* path, cam_settings_t* cfg);
int cam_cap(void* buf);

#endif
