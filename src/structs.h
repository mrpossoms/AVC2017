#ifndef AVC_STRUCTS
#define AVC_STRUCTS

#define FRAME_W 160
#define FRAME_H 120
#define PIX_DEPTH 2

#define VIEW_PIXELS (FRAME_W * FRAME_H)

typedef union {
	struct {
		uint8_t r, g, b;
	};
	uint8_t v[3];
} color_t;

typedef struct {
	uint8_t throttle, steering;
} raw_action_t;

typedef struct {
	int16_t rot_rate[3];
	int16_t acc[3];
	int8_t vel;
	uint32_t distance;
	uint8_t view[FRAME_W * FRAME_H * PIX_DEPTH];
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
		float luma[VIEW_PIXELS];
		float chroma[VIEW_PIXELS / 4];
	};
	float v[3 + 3 + 2 + VIEW_PIXELS + VIEW_PIXELS / 4];
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

#define VEC_DIMENSIONS_F(v) (sizeof((v)) / sizeof(float))

#endif
