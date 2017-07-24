#ifndef AVC_STRUCTS
#define AVC_STRUCTS

#include <sys/types.h>
#include <inttypes.h>

#define FRAME_W 80
#define FRAME_H 60
#define PIX_DEPTH 3

#define VIEW_PIXELS (FRAME_W * FRAME_H)

typedef union {
	struct {
		uint8_t r, g, b;
	};
	struct {
		uint8_t y, cb, cr;
	};
	uint8_t v[3];
} color_t;

typedef union {
	struct {
		float r, g, b;
	};
	struct {
		float y, cb, cr;
	};
	float v[3];
} color_f_t;

typedef struct {
	uint8_t throttle, steering;
} raw_action_t;

typedef struct {
	int16_t rot_rate[3];
	int16_t acc[3];
	int8_t vel;
	uint32_t distance;
	color_t view[FRAME_W * FRAME_H];
} raw_state_t;

typedef struct {
	raw_state_t state;
	raw_action_t action;
} raw_example_t;

typedef union {
	struct {
		float rot[3];
		float acc[3];
		float vel;
		float distance;
		color_f_t view[FRAME_W * FRAME_H];
	};
	float v[3 + 3 + 2 + VIEW_PIXELS * 3];
} state_f_t;

typedef union {
	struct {
		float throttle[7];
		float steering[15];
	};
	float v[22];
} action_f_t;

typedef struct {
	state_f_t  state;
	action_f_t action;
} example_t;

typedef struct {
	float min, max;
} range_t;

typedef struct {
	range_t steering;
	range_t throttle;
} calib_t;

#define VEC_DIMENSIONS_F(v) (sizeof((v)) / sizeof(float))

#endif
