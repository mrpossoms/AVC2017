#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <pthread.h>

#include <GLFW/glfw3.h>

#include "structs.h"
#include "sys.h"

// #define RENDER_DEMO

#ifdef __linux__
#define WIN_W (640)
#define WIN_H (480)
#else
#define WIN_W (640 >> 1)
#define WIN_H (480 >> 1)
#endif

GLFWwindow* WIN;
GLuint frameTex;
int USE_SLEEP;
int FORWARD_STATE;

static void setupGL()
{
	glShadeModel(GL_SMOOTH);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glEnable(GL_TEXTURE_2D);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
}


static void createTexture(GLuint* tex)
{
	glGenTextures(1, tex);
	glBindTexture(GL_TEXTURE_2D, *tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
}


void draw_path_travelled(raw_state_t* state, vec3* positions)
{
	glDisable(GL_TEXTURE_2D);

	const float scale_factor = 0.02f;
	glColor3f(1, 0, 0);
	glBegin(GL_POINTS);
		for(int i = 1024; i--;)
		{
			glVertex2f(positions[i][0] * scale_factor, positions[i][1] * scale_factor);
		}
	glEnd();

	glColor3f(0, 1, 0);
	glBegin(GL_LINES);
			glVertex2f(
				(state->position[0]) * scale_factor,
				(state->position[1]) * scale_factor
			);
			glVertex2f(
				(state->position[0] + state->heading[0]) * scale_factor,
				(state->position[1] + state->heading[1]) * scale_factor
			);
	glEnd();
}

void frame_to_canon(float x_frame, float y_frame, float* x, float* y)
{
	*x = ((x_frame / (float)WIN_W) - 0.5) * 2;
	*y = ((y_frame / (float)WIN_H) - 0.5) * -2;
}

void frame_to_pix(float x_frame, float y_frame, int* x, int* y)
{
	*x = (x_frame / (float)WIN_W) * FRAME_W;
	*y = (y_frame / (float)WIN_H) * FRAME_H;
}

int LABEL_CLASS;
char* BASE_PATH;

int main(int argc, char* argv[])
{
	PROC_NAME = argv[0];

	cli_cmd_t cmds[] = {
		{ 'c',
			.desc = "Specify class number for saving images",
			.set = &LABEL_CLASS,
			.type = ARG_TYP_INT,
			.opts = { .has_value = 1 },
		},
		{ 'p',
			.desc = "Set base path",
			.set = &BASE_PATH,
			.type = ARG_TYP_STR,
			.opts = { .has_value = 1 },
		},
		{ 'd',
			.desc = "Apply slight delay",
			.set = &USE_SLEEP,
			.type = ARG_TYP_INT,
			.opts = { .has_value = 1 },
		},
		{ 'f',
			.desc = "Forward full system state over stdout",
			.set = &FORWARD_STATE,
		},
		{}
	};
	const char* prog_desc = "";

	if (cli(prog_desc, cmds, argc, argv))
	{
		return -2;
	}

	if (!glfwInit()){
		return -1;
	}

	b_log("Magic %d\n", MAGIC);

	WIN = glfwCreateWindow(WIN_W, WIN_H, "AVC 2017", NULL, NULL);

	if (!WIN){
		glfwTerminate();
		return -2;
	}

	srandom(time(NULL));
	glfwMakeContextCurrent(WIN);
	setupGL();
	createTexture(&frameTex);

	color_t rgb[FRAME_W * FRAME_H] = {};
	message_t msg = {};

	vec3 positions[1024];
	int pos_idx = 0;
	int img_fd = 0;

	if(argc >= 2)
	{
		img_fd = open(argv[1], O_RDONLY);
		//USE_SLEEP = 1;
	}

	raw_state_t* state = NULL;

	while(!glfwWindowShouldClose(WIN)){
		glTexImage2D(
			GL_TEXTURE_2D,
			0,
			GL_RGB,
			FRAME_W,
			FRAME_H,
			0,
			GL_RGB,
			GL_UNSIGNED_BYTE,
			rgb
		);

		int space_down = glfwGetKey(WIN, GLFW_KEY_SPACE) == GLFW_PRESS;

		if (!space_down)
		if (read_pipeline_payload(&msg, PAYLOAD_STATE))
		{
			return -1;
		}

		state = &msg.payload.state;
		if (msg.header.type == PAYLOAD_PAIR)
		{
			state = &msg.payload.pair.state;
		}

		vec3_copy(positions[pos_idx++], state->position);
		if(pos_idx == 1024) b_log("ROLLOVER");
		pos_idx %= 1024;
		yuv422_to_rgb(state->view.luma, state->view.chroma, rgb, FRAME_W, FRAME_H);

		glClear(GL_COLOR_BUFFER_BIT);
		glEnable(GL_TEXTURE_2D);

		glBegin(GL_QUADS);
			glTexCoord2f(1, 0);
			glVertex2f( 1,  1);

			glTexCoord2f(0, 0);
			glVertex2f(-1,  1);

			glTexCoord2f(0, 1);
			glVertex2f(-1, -1);

			glTexCoord2f(1, 1);
			glVertex2f( 1, -1);
		glEnd();

		draw_path_travelled(state, positions);

		if (BASE_PATH)
		{
			if (glfwGetMouseButton(WIN, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
			{
				double x, y;
				glfwGetCursorPos(WIN, &x, &y);

				float ul[2];
				float lr[2];

				frame_to_canon(x, y, ul + 0, ul + 1);
				frame_to_canon(x + 16, y + 16, lr + 0, lr + 1);

				rectangle_t patch_rec = { .w = 16, .h = 16 };
				color_t patch[16 * 16];
				frame_to_pix(x, y, &patch_rec.x, &patch_rec.y);
				image_patch_b(patch, rgb, patch_rec);

				char file_path[PATH_MAX] = {}, base_path[PATH_MAX] = {};
				snprintf(base_path, PATH_MAX, "%s/%d", BASE_PATH, LABEL_CLASS);
				mkdir(base_path, 0777);
				snprintf(file_path, PATH_MAX, "%s/%lx", base_path, random());
				write_png_file_rgb(file_path, patch_rec.w, patch_rec.h, (char*)patch);

				glBegin(GL_QUADS);
					glColor4f(1, 0, 0, 0.5);
					glVertex2f(ul[0], ul[1]);
					glVertex2f(lr[0], ul[1]);
					glVertex2f(lr[0], lr[1]);
					glVertex2f(ul[0], lr[1]);
				glEnd();
			}
		}

		glfwPollEvents();
		glfwSwapBuffers(WIN);

		if (!space_down)
		if(USE_SLEEP) usleep(1000 * USE_SLEEP);

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
