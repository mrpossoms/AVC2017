#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "seen.hpp"
#include "sky.hpp"
#include "json.hpp"

#include "sys.h"

using namespace seen;
using namespace nlohmann;

#include "drawables.hpp"
#include "helpers.hpp"

#define SURFACE_SHADER {                      \
	.vertex = "displacement.vsh",             \
	.tessalation = {                          \
		.control = "displacement.tcs",        \
		.evaluation = "displacement.tes",     \
	},                                        \
	.geometry = "",                           \
	.fragment = "basic.fsh" }                 \


static vec3_t one = { 1, 1, 1 };

static vec4_t material = { 0.1, 0.01, 1, 0.01 };
static vec3_t light_dir = { 1, -1, 1 };
static vec3_t tex_control = { 0, 0, 16 };

struct {
	Vec3 position = { 0, -0.3, 0 };
	float heading;
	float speed;
	float friction = 0.2;

	void update()
	{
		speed *= 1 - friction;
		Vec3 dir(cos(heading + M_PI / 2) * speed, 0, sin(heading + M_PI / 2) * speed);
		position = position + dir;
	}

	void turn(float d_theta)
	{
		heading += d_theta * speed;
	}

	void accelerate(float d_v)
	{
		speed += d_v;
	}

	Quat orientation()
	{
		Quat q;
		quat_from_axis_angle(q.v, 0, 1, 0, heading);
		return q;
	}
} vehicle;


void poll_ctrl_sock(int sock)
{
	fd_set fds;
	struct timeval tout = { 0, 1000 * 16 };
	FD_ZERO(&fds);
	FD_SET(sock, &fds);

	int res = select(sock + 1, &fds, NULL, NULL, &tout);

	switch(res)
	{
		case 0: // timeout occurred
		break;
		case -1: // error
		break;
		default:
		if (FD_ISSET(sock, &fds))
		{
			printf("Something on the socket!\n");
		}
	}
}


int main (int argc, char* argv[])
{
	std::ifstream i("scene.json");

	RendererGL renderer("./data", "Sim", FRAME_W >> 1, FRAME_H >> 1);
	Camera camera(M_PI / 2, renderer.width, renderer.height);

	ListScene scene;

	json scene_json;
	i >> scene_json;

	// Sky setup
	Sky sky;
	CustomPass sky_pass([&]() {
		// draw pass preparation
		ShaderConfig shader_desc = SKY_SHADERS;
		ShaderProgram* shader = Shaders[shader_desc]->use();
	}, &sky, NULL);


	// Asphalt setup
	Asphalt asphalt;

	CustomPass ground_pass([&]() {
		// draw pass preparation
		ShaderConfig shader_desc = SURFACE_SHADER;
		ShaderProgram& shader = *Shaders[shader_desc]->use();

		shader["u_light_dir"] << light_dir;
		shader["u_displacement_weight"] << 0.1f;
		shader["u_tex_control"] << tex_control;
		shader["TessLevelInner"] << 5.0f;
		shader["TessLevelOuter"] << 5.0f;
		shader["u_tint"] << one;

		shader["us_displacement"] << asphalt.disp_tex;
		shader << asphalt.mat;
	}, &asphalt, NULL);

	CustomPass bale_pass([&]() {
		ShaderConfig shader_desc = SURFACE_SHADER;
		ShaderProgram& shader = *Shaders[shader_desc]->use();

		shader["TessLevelInner"] << 5.0f;
		shader["TessLevelOuter"] << 5.0f;
		shader["us_displacement"] << HayBale::displacement_tex();
		shader << HayBale::material();
	}, NULL);

	mat4x4_t I;
	mat4x4_identity(I.v);
	mat4x4_translate_in_place(I.v, 0, 1, 0);
	auto root = scene_json["object"];
	populate_scene(bale_pass, root, I);

	camera.position(0, -1.1, 0);
	scene.drawables().push_back(&sky_pass);
	scene.drawables().push_back(&ground_pass);
	scene.drawables().push_back(&bale_pass);

	int sock_fd = open_ctrl_socket();

	float t = 0;
	renderer.key_pressed = [&](int key) {

		switch (key) {
			case GLFW_KEY_LEFT:
				vehicle.turn(-0.1);
				break;
			case GLFW_KEY_RIGHT:
				vehicle.turn(0.1);
				break;
			case GLFW_KEY_UP:
			{
				vehicle.accelerate(0.1);
			}
				break;
			case GLFW_KEY_DOWN:
			{
				vehicle.accelerate(-0.1);
			}
				break;
		}
	};

	dataset_header_t hdr = { MAGIC, 1 };
	write(1, &hdr, sizeof(hdr));

	while (renderer.is_running())
	{
		// look for socket input
		poll_ctrl_sock(sock_fd);

		Quat q = vehicle.orientation();
		vehicle.update();
		camera.position(vehicle.position);
		camera.orientation(q);

		renderer.draw(&camera, &scene);

		raw_example_t ex;

		color_t rgb_buf[FRAME_W * FRAME_H], tmp[FRAME_W * FRAME_H];
		glReadPixels(0, 0, FRAME_W, FRAME_H, GL_RGB, GL_UNSIGNED_BYTE, (void*)tmp);

		for(int i = FRAME_H; i--;)
		{
			memcpy(rgb_buf + (i * FRAME_W), tmp + ((FRAME_H - i) * FRAME_W), sizeof(color_t) * FRAME_W);
		}

		rgb_to_yuv422(ex.state.view.luma, ex.state.view.chroma, rgb_buf, FRAME_W, FRAME_H);

		write(1, &ex, sizeof(ex));
	}

	close(sock_fd);
	unlink("/tmp/avc.sim.ctrl");

	return 0;
}
