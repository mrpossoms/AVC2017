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

// #define RENDER_DEMO

GLFWwindow* WIN;
GLuint frameTex;
size_t frameBufferSize;
char*  frameBuffer = NULL;

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

float clamp(float v)
{
	v = v > 255 ? 255 : v;
	return  v < 0 ? 0 : v;
}

void yuv422_to_rgb(color_t* yuv, color_t* rgb, int w, int h)
{
	for(int i = w * h; i--;)
	{
		rgb[i].r = clamp(yuv[i].y + 1.14 * (yuv[i].cb - 128));
		rgb[i].g = clamp(yuv[i].y - 0.395 * (yuv[i].cr - 128) - (0.581 * (yuv[i].cb - 128)));
		rgb[i].b = clamp(yuv[i].y + 2.033 * (yuv[i].cr - 128));
	}
}

static int oneOK;
int main(int argc, char* argv[])
{
	pthread_t rcThread;

	if (!glfwInit()){
		return -1;
	}

	fprintf(stderr, "raw_state_t: %luB, raw_action_t: %luB\n", sizeof(raw_state_t), sizeof(raw_action_t));

	WIN = glfwCreateWindow(640, 480, "AVC 2017", NULL, NULL);

	if (!WIN){
		glfwTerminate();
		return -2;
	}

	glfwMakeContextCurrent(WIN);
	setupGL();
	createTexture(&frameTex);

	uint8_t lum[FRAME_W * FRAME_H];
	color_t yuv[FRAME_W * FRAME_H];
	color_t rgb[FRAME_W * FRAME_H];
	raw_example_t ex = {};

	int img_fd = open(argv[1], O_RDONLY);
	oneOK = (read(img_fd, &ex, sizeof(ex)) == sizeof(ex));

	// yuv422_to_lum8(yuv, lum, FRAME_W, FRAME_H);
	yuv422_to_rgb(yuv, rgb, FRAME_W, FRAME_H);

	while(!glfwWindowShouldClose(WIN)){

	int res = 1;
	if(!oneOK)
	{
		static int rand_fd;

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
	else
	{
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

		oneOK = (read(img_fd, &ex, sizeof(ex)) == sizeof(ex));
		// yuv422_to_lum8(yuv, lum, FRAME_W, FRAME_H);
		yuv422_to_rgb(ex.state.view, rgb, FRAME_W, FRAME_H);

		usleep(500000);
	}

		glClear(GL_COLOR_BUFFER_BIT);
		glEnable(GL_TEXTURE_2D);
		glBegin(GL_QUADS);
			glVertex2f( 1,  1);
			glTexCoord2f(0, 0);

			glVertex2f(-1,  1);
			glTexCoord2f(0, 1);

			glVertex2f(-1, -1);
			glTexCoord2f(1, 1);

			glVertex2f( 1, -1);
			glTexCoord2f(1, 0);
		glEnd();

		glfwPollEvents();
		glfwSwapBuffers(WIN);
	}

	return 0;
}
