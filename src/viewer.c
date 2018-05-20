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

GLFWwindow* WIN;
GLuint frameTex;

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

void yuv422_to_lum8(color_t* yuv, uint8_t* lum8, int w, int h)
{
	for(int i = w * h; i--;)
	{
		lum8[i] = yuv[i].y;
	}
}


void display_static()
{
	static int rand_fd;
	static uint8_t lum[FRAME_W * FRAME_H];

	if(!rand_fd)
	{
		rand_fd = open("/dev/random", O_RDONLY);
	}

	read(rand_fd, lum, VIEW_PIXELS);

	glTexImage2D(
		GL_TEXTURE_2D,
		0,
		GL_LUMINANCE, // one color channel
		FRAME_W,
		FRAME_H,
		0, // no border
		GL_LUMINANCE,
		GL_UNSIGNED_BYTE,
		lum
	);
}


int main(int argc, char* argv[])
{
	PROC_NAME = argv[0];

	if (!glfwInit()){
		return -1;
	}

	b_log("Magic %d\n", MAGIC);

	WIN = glfwCreateWindow(640, 480, "AVC 2017", NULL, NULL);

	if (!WIN){
		glfwTerminate();
		return -2;
	}

	glfwMakeContextCurrent(WIN);
	setupGL();
	createTexture(&frameTex);

	color_t rgb[FRAME_W * FRAME_H] = {};
	message_t msg = {};

	vec3 positions[1024];
	int pos_idx = 0;
	int use_sleep = 0;

	if(argc >= 2)
	{
		img_fd = open(argv[1], O_RDONLY);
		//use_sleep = 1;
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

		glDisable(GL_TEXTURE_2D);

		glColor3f(1, 0, 0);
		glBegin(GL_LINE_STRIP);
			for(int i = 1024; i--;)
			{
				glVertex2f(positions[i][0] / 10.f, positions[i][1] / 10.f);
			}
		glEnd();

		glColor3f(0, 1, 0);
		glBegin(GL_LINES);
				glVertex2f(
					(state->position[0]) / 10.f,
					(state->position[1]) / 10.f
				);
				glVertex2f(
					(state->position[0] + state->heading[0]) / 10.f,
					(state->position[1] + state->heading[1]) / 10.f
				);
		glEnd();

		glfwPollEvents();
		glfwSwapBuffers(WIN);

		if(use_sleep) usleep(1000 * 250);
	}

	return 0;
}
