#ifndef AVC_STRUCTS
#define AVC_STRUCTS

#include <sys/types.h>
#include <inttypes.h>
#include <errno.h>
#include "linmath.h"

#define EXIT(...) {\
	fprintf(stderr, __VA_ARGS__);\
	fprintf(stderr, " (%d)\n", errno);\
	exit(-1);\
}\

#define FRAME_W 320
#define FRAME_H 240
#define PIX_DEPTH 3

#define VIEW_PIXELS (FRAME_W * FRAME_H)
#define LUMA_PIXELS (FRAME_W * FRAME_H)
#define CHRO_PIXELS (FRAME_W / 2 * FRAME_H)


typedef union {
	struct {
		uint8_t cb, cr;
	};
	uint8_t v[2];
} chroma_t;

typedef union {
	struct {
		float cb, cr;
	};
	float v[2];
} chroma_f_t;

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
	int16_t  rot_rate[3];
	int16_t  acc[3];
	float    vel;
	float    distance;
	vec3     heading;
	vec3     position;
	struct {
		uint8_t luma[LUMA_PIXELS];
		chroma_t chroma[CHRO_PIXELS];
	} view;
} raw_state_t;


struct waypoint;
typedef struct waypoint {
	vec3 position;
	vec3 heading;
	float velocity;
	struct waypoint* next;
} waypoint_t;

typedef struct {
	float min, max;
} range_t;

typedef struct {
	range_t steering;
	range_t throttle;
} calib_t;

/**
 * Payloads
 */

typedef enum {
	PAYLOAD_STATE  = 0x01,
	PAYLOAD_ACTION = 0x02,
	PAYLOAD_PAIR   = 0x03,
} payload_type_t;

typedef struct {
	uint64_t magic;
	payload_type_t type;
} dataset_hdr_t;

typedef struct {
	dataset_hdr_t header;
	union {
		raw_state_t state;
		raw_action_t action;
		struct {
			raw_action_t action;
			raw_state_t state;
		} pair;
	} payload;
} message_t;

#define VEC_DIMENSIONS_F(v) (sizeof((v)) / sizeof(float))

#endif
