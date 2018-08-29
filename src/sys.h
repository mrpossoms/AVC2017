#ifndef AVC_SYS
#define AVC_SYS

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <getopt.h>
#include <sched.h>
#include <pthread.h>

#include "structs.h"

#define AVC_TERM_GREEN "\033[0;32m"
#define AVC_TERM_RED "\033[1;31m"
#define AVC_TERM_COLOR_OFF "\033[0m"

#define ACTION_CAL_PATH "/etc/bot/actuator/actions.cal"
#define RESTING_PWM_PATH "resting"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(x, min, max) ((x) < (max) ? ((x) > (min) ? (x) : (min)) : (max))

extern const char* PROC_NAME;

typedef struct {
    int x, y, w, h;
} rectangle_t;

typedef struct {
	char c;
	const char* str;
} cli_flag_t;

typedef enum {
	ARG_TYP_FLAG = 0,
	ARG_TYP_INT,
	ARG_TYP_STR,
	ARG_TYP_CALLBACK
} cli_arg_type_t;

typedef struct {
	char flag;
	const char* desc;
	const char* usage;
	struct {
		int required;
		int has_value;
	} opts;
	void* set;
	cli_arg_type_t type;
	int _present;
} cli_cmd_t;

typedef struct {
	struct timeval start;
	uint32_t interval_us;
} timegate_t;

void b_log(const char* fmt, ...);
void b_good(const char* fmt, ...);
void b_bad(const char* fmt, ...);

long diff_us(struct timeval then, struct timeval now);
void timegate_open(timegate_t* tg);
void timegate_close(timegate_t* tg);

int write_pipeline_payload(message_t* msg);
int read_pipeline_payload(message_t* msg, payload_type_t exp_type);

int calib_load(const char* path, calib_t* cal);

float clamp(float v);

int path_exists(const char* path);

void cli_help(char* const argv[], const char* prog_desc, const char* cmds, const char* cmd_desc[]);
int cli(const char* prog_desc, cli_cmd_t commands[], int argc, char* const argv[]);

#endif
