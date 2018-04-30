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

#include "dataset_hdr.h"
#include "structs.h"

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

float clamp(float v)
{
	v = v > 255 ? 255 : v;
	return  v < 0 ? 0 : v;
}

void yuv422_to_rgb(uint8_t* luma, chroma_t* uv, color_t* rgb, int w, int h)
{
	for(int yi = h; yi--;)
	for(int xi = w; xi--;)
	{
		int i = yi * w + xi;
		int j = yi * (w >> 1) + (xi >> 1);

		rgb[i].r = clamp(luma[i] + 1.14 * (uv[j].cb - 128));
		rgb[i].g = clamp(luma[i] - 0.395 * (uv[j].cr - 128) - (0.581 * (uv[j].cb - 128)));
		rgb[i].b = clamp(luma[i] + 2.033 * (uv[j].cr - 128));
	}
}

void next_example(int fd, raw_example_t* ex)
{
	size_t needed = sizeof(raw_example_t);
	off_t  off = 0;
	uint8_t* buf = (uint8_t*)ex;

	while(needed)
	{
		size_t gotten = read(fd, buf + off, needed);
		needed -= gotten;
		off += gotten;
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


static int oneOK;
int main(int argc, char* argv[])
{
	if (!glfwInit()){
		return -1;
	}

	fprintf(stderr, "Magic %d\n", MAGIC);

	WIN = glfwCreateWindow(640, 480, "AVC 2017", NULL, NULL);

	if (!WIN){
		glfwTerminate();
		return -2;
	}

	glfwMakeContextCurrent(WIN);
	setupGL();
	createTexture(&frameTex);

	color_t rgb[FRAME_W * FRAME_H] = {};
	raw_example_t ex = {};

	vec3 positions[1024];
	int pos_idx = 0;
	int use_sleep = 0;
	int img_fd = 0;

	if(argc >= 2)
	{
		img_fd = open(argv[1], O_RDONLY);
		//use_sleep = 1;
	}

	dataset_header_t hdr = {};
	int oneOK = read(img_fd, &hdr, sizeof(hdr)) == sizeof(hdr);

	if(hdr.magic != ((uint64_t)MAGIC))
	{
		EXIT("Incompatible version");
	}

	next_example(img_fd, &ex);
	yuv422_to_rgb(ex.state.view.luma, ex.state.view.chroma, rgb, FRAME_W, FRAME_H);

	while(!glfwWindowShouldClose(WIN)){

		if(!oneOK)
		{
			display_static();
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

			next_example(img_fd, &ex);
			vec3_copy(positions[pos_idx++], ex.state.position);
			if(pos_idx == 1024) printf("ROLLOVER\n");
			pos_idx %= 1024;
			yuv422_to_rgb(ex.state.view.luma, ex.state.view.chroma, rgb, FRAME_W, FRAME_H);
		}

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
					(ex.state.position[0]) / 10.f,
					(ex.state.position[1]) / 10.f
				);
				glVertex2f(
					(ex.state.position[0] + ex.state.heading[0]) / 10.f,
					(ex.state.position[1] + ex.state.heading[1]) / 10.f
				);
		glEnd();

		glfwPollEvents();
		glfwSwapBuffers(WIN);

		if(use_sleep) usleep(1000 * 250);
	}

	return 0;
}
