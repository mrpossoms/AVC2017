#include <stdio.h>
#include <stdarg.h>
#include "sys.h"
#include "structs.h"
#include "vision.h"


int FORWARD_STATE;
int RUNNING = 1;
int TINT;
char* CLASS_PATH;
char* CLASS_NAME;
char FULL_PATH[PATH_MAX];

struct {
	int x1, y1, x2, y2;
	int w, h;
} CAP_WIN;


static int arg_parse_capture_win(char f, char* v)
{
	int* w = &CAP_WIN.x1;
	if (sscanf(v, "%d,%d,%d,%d", w, w + 1, w + 2, w + 3) != 4)
	{
		return -1;
	}

	if (w[0] == -1 && w[1] == -1)
	{
		CAP_WIN.w = CAP_WIN.x2;
		CAP_WIN.h = CAP_WIN.y2;
		CAP_WIN.x1 = (FRAME_W >> 1) - (CAP_WIN.w >> 1);
		CAP_WIN.y1 = (FRAME_H >> 1) - (CAP_WIN.h >> 1);
	}
	else
	{
		CAP_WIN.w = CAP_WIN.x2 - CAP_WIN.x1;
		CAP_WIN.h = CAP_WIN.y2 - CAP_WIN.y1;
	}

	if (CAP_WIN.w < 0 || CAP_WIN.h < 0)
	{
		if (CAP_WIN.w < 0)
		{
			b_bad("Capture window width is negative.");
		}

		if (CAP_WIN.h < 0)
		{
			b_bad("Capture window height is negative.");
		}

		b_bad("Are your upper-left, and lower-right flipped?");
		return -2;
	}

	return 0;
}


void sig_handler(int sig)
{
	b_log("Caught signal %d", sig);
	RUNNING = 0;
}


int main (int argc,  char* const argv[])
{
	PROC_NAME = argv[0];
	signal(SIGINT, sig_handler);

	cli_cmd_t cmds[] = {
		{ 'f',
			.desc = "Forward full system state over stdout",
			.set = &FORWARD_STATE,
		},
		{ 'p',
			.desc = "Dataset path which contains class directories where images will be saved.",
			.usage = "-p [dataset_path]",
			.opts = { .required = 1, .has_value = 1 },
			.set = &CLASS_PATH,
			.type = ARG_TYP_STR
		},
		{ 'c',
			.desc = "Class name which all collected images will be filed under",
			.usage = "-c [class_name]",
			.opts = { .required = 1, .has_value = 1 },
			.set = &CLASS_NAME,
			.type = ARG_TYP_STR
		},
		{ 'w',
			.desc = "Region of the frame to be captured (units in pixels)",
			.usage = "-w [x1],[y1],[x2],[y2] where 1 denotes the top left, and 2 denotes the bottom right",
			.opts = { .required = 1, .has_value = 1 },
			.set = arg_parse_capture_win,
			.type = ARG_TYP_CALLBACK
		},
		{ 't',
			.desc = "Tint the region that is sampled",
			.set = &TINT
		},
		{}
	};
	const char* prog_desc = "Captures the video feed and crops out a small region which is then saved as an image for use as training data.";

	if (cli(prog_desc, cmds, argc, argv))
	{
		return -2;
	}

	// Check to ensure that the dataset base path exists
	if (path_exists(CLASS_PATH) == 0)
	{
		b_bad("Provided dataset path is not accessible");
		return -3;
	}

	// build the full class path
	snprintf(FULL_PATH, PATH_MAX, "%s/%s", CLASS_PATH, CLASS_NAME);

	// check the class directory, if it doesn't exist, but we are
	// in a vaild dataset directory tree, then create the class directory
	if (path_exists(FULL_PATH) == 0)
	{
		b_log("creating '%s' directory and continuing", FULL_PATH);
		if (mkdir(FULL_PATH, 0777))
		{
			b_bad("Creation failed");
			return -4;
		}
	}

	while (RUNNING)
	{
		message_t msg = {};
		color_t rgb[FRAME_W * FRAME_H];
		color_t cap[CAP_WIN.w * CAP_WIN.h];

		if (read_pipeline_payload(&msg, PAYLOAD_PAIR))
		{
			b_bad("Read error");
			return -5;
		}

		raw_state_t* state = &msg.payload.state;
		yuv422_to_rgb(state->view.luma, state->view.chroma, rgb, FRAME_W, FRAME_H);
		// int rf = open("/dev/random", O_RDONLY);
		// read(rf, rgb, sizeof(rgb));
		// close(rf);

		size_t frame_row_size = FRAME_W;
		size_t cap_row_size = CAP_WIN.w;
		for (int row = CAP_WIN.h; row--;)
		{
			memcpy(
				cap + (row * cap_row_size),
				rgb + ((row + CAP_WIN.y1) * frame_row_size) + CAP_WIN.x1,
				sizeof(color_t) * cap_row_size
			);

			if (TINT)
			memset(
				state->view.luma + ((row + CAP_WIN.y1) * frame_row_size) + CAP_WIN.x1,
				255,
				sizeof(uint8_t) * cap_row_size
			);
		}


		char file_path[PATH_MAX] = {};
		snprintf(file_path, PATH_MAX, "%s/%lx", FULL_PATH, random());
		write_png_file_rgb(file_path, CAP_WIN.w, CAP_WIN.h, (const char*)cap);

		if (FORWARD_STATE)
		{
			if (write_pipeline_payload(&msg))
			{
				b_bad("Failed to write payload");
				return -1;
			}
		}
	}

	return 0;
}
