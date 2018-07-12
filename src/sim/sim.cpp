#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "seen.hpp"
#include "sky.hpp"
#include "json.hpp"

#include "sys.h"

using namespace seen;
using namespace nlohmann;

#include "drawables.hpp"
#include "helpers.hpp"

static vec3_t one = { 1, 1, 1 };

static vec4_t material = { 0.1, 0.01, 1, 0.01 };
static vec3_t light_dir = { 1, -1, 1 };
static vec3_t tex_control = { 0, 0, 16 };

bool PAUSED = false;
bool STEER_LOCKED = false;

struct {
	Vec3 position = { 0, -1, 0 };
	float angle;
	float speed;
	float distance;
	float friction = 0.2;
	float steering_angle;

	void update()
	{
		speed *= 1 - friction;
		Vec3 dir = heading();
		position = position + (dir * speed);
		angle += steer_speed();
		distance += speed;
	}

	Vec3 heading()
	{
		Vec3 dir(cos(angle - M_PI / 2), 0, sin(angle - M_PI / 2));
		return dir;
	}

	float steer_speed()
	{
		return steering_angle * speed;
	}

	void turn(float d_theta)
	{
		steering_angle = d_theta;
	}

	void accelerate(float d_v)
	{
		speed += d_v;
	}

	Quat orientation()
	{
		Quat q;
		quat_from_axis_angle(q.v, 0, 1, 0, angle);
		return q;
	}
} vehicle;


void poll_ctrl_pipe(int sock)
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
			raw_action_t act;
			if (read(sock, &act, sizeof(act)) == sizeof(act))
			{
				if (!STEER_LOCKED)
				{
					vehicle.turn((act.steering - 128.f) / 512.f);
				}

				vehicle.accelerate((act.throttle - 128.f) / 1024.f);
			}
		}
	}
}


int main (int argc, char* argv[])
{
	std::ifstream i("scene.json");

	RendererGL renderer("./data", "Sim", FRAME_W, FRAME_H, 4, 0);
	Camera camera(M_PI / 2, renderer.width, renderer.height);

	ListScene scene, ground_scene, hay_scene;

	json scene_json;
	i >> scene_json;

	// Sky setup
	seen::Model* sky = seen::MeshFactory::get_model("sphere.obj");
	CustomPass sky_pass([&](int index) {
		// draw pass preparation
		//ShaderConfig shader_desc = SKY_SHADERS;
		//ShaderProgram* shader = Shaders[shader_desc]->use();
		seen::ShaderProgram::builtin_sky()->use();
		glDisable(GL_CULL_FACE);
	}, NULL);

	scene.drawables().push_back(sky);
	sky_pass.scene = &scene;


	// Asphalt setup
	Asphalt asphalt;

	auto vsh = seen::Shader::vertex("basic_vsh");
	vsh.vertex(seen::Shader::VERT_POSITION | seen::Shader::VERT_NORMAL | seen::Shader::VERT_TANGENT | seen::Shader::VERT_UV)
	   .transformed()
   	   .compute_binormal()
	   .viewed().projected().pass_through("texcoord_in")
	   .emit_position()
	   .next(vsh.builtin("gl_Position") = vsh.local("l_pos_proj"));

	auto fsh = seen::Shader::fragment("basic_fsh").preceded_by(vsh);
	fsh.color_textured()
	   .normal_mapped()
	//    .shadow_mapped_vsm()
	   .blinn()
	;

	auto primary_shader = seen::ShaderProgram::compile({ vsh, fsh });
	seen::Light light = {
		.position = { 0, 20, 0 },
		.power = { 2.5, 2.5, 2.5 },
		.is_point = true,
		.ambience = 0.01
	};

	CustomPass ground_pass([&](int index) {
		// draw pass preparation
		// ShaderConfig shader_desc = SURFACE_SHADER;
		// ShaderProgram& shader = *Shaders[shader_desc]->use();
		//
		// shader["u_light_dir"] << light_dir;
		// shader["u_displacement_weight"] << 0.1f;
		// shader["u_tex_control"] << tex_control;
		// shader["TessLevelInner"] << 5.0f;
		// shader["TessLevelOuter"] << 5.0f;
		// shader["u_tint"] << one;
		//
		// shader["us_displacement"] << asphalt.disp_tex;
		primary_shader->use();

		*primary_shader << &light;
		*primary_shader << asphalt.mat;
	}, NULL);
	ground_scene.drawables().push_back(&asphalt);
	ground_pass.scene = &ground_scene;

	CustomPass bale_pass([&](int index) {
		// ShaderConfig shader_desc = SURFACE_SHADER;
		// ShaderProgram& shader = *Shaders[shader_desc]->use();
		//
		// shader["TessLevelInner"] << 5.0f;
		// shader["TessLevelOuter"] << 5.0f;
		// shader["us_displacement"] << HayBale::displacement_tex();
		primary_shader->use();

		*primary_shader << HayBale::material();
	}, NULL);
	bale_pass.scene = &hay_scene;

	mat4x4_t I;
	mat4x4_identity(I.v);
	mat4x4_translate_in_place(I.v, 0, 0, 0);
	auto root = scene_json["object"];
	populate_scene(hay_scene, root, I);

	// camera.position(0, 0, 0);

	int sock_fd = open_ctrl_pipe();

	float t = 0;

	renderer.key_pressed = [&](int key) {
		switch (key) {
			case GLFW_KEY_L:
				STEER_LOCKED = true;
				break;
			case GLFW_KEY_LEFT:
				vehicle.turn(-0.2);
				break;
			case GLFW_KEY_RIGHT:
				vehicle.turn(0.2);
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
			case GLFW_KEY_SPACE:
			{
				PAUSED = true;
			}
				break;
		}
	};

	renderer.key_released = [&](int key) {
		switch (key) {
			case GLFW_KEY_LEFT:
			case GLFW_KEY_RIGHT:
				vehicle.turn(0);
			case GLFW_KEY_SPACE:
			{
				PAUSED = false;
			}
				break;
			case GLFW_KEY_L:
				STEER_LOCKED = false;
				break;
		}
	};

	message_t msg = {
		.header = {
			.magic = MAGIC,
			.type  = PAYLOAD_STATE
		},
	};

	while (renderer.is_running())
	{
		// look for socket input
		poll_ctrl_pipe(sock_fd);

		Quat q = vehicle.orientation();
		Quat tilt, roll, jitter;
		quat_from_axis_angle(tilt.v, 1, 0, 0, 0.35);
		quat_from_axis_angle(roll.v, 0, 0, 1, -1.0f * vehicle.speed * vehicle.steer_speed());

		Vec3 ja = seen::rn(); // jitter axis
		quat_from_axis_angle(jitter.v, ja.x, ja.y, ja.z, seen::rf(-0.01, 0.01) * vehicle.speed);

		if (!PAUSED)
		{
			vehicle.update();
			camera.position(vehicle.position);
			q = tilt * roll * jitter * q;
			camera.orientation(q);

			Vec3 heading = vehicle.heading();
			raw_state_t state = {
				.vel = vehicle.speed,
				.distance = vehicle.distance,
				.heading = { heading.x, heading.z, heading.y },
				.position = { vehicle.position.x, vehicle.position.z, vehicle.position.y }
			};

			color_t rgb_buf[FRAME_W * FRAME_H], tmp[FRAME_W * FRAME_H];
			glReadPixels(0, 0, FRAME_W, FRAME_H, GL_RGB, GL_UNSIGNED_BYTE, (void*)tmp);

			// Add clamped noise to the image
			for (int i = FRAME_W * FRAME_H * 3; i--;)
			{
				int c = ((uint8_t*)tmp)[i] + ((random() % 16) - 8);
				if (c < 0) c = 0;
				if (c > 255) c = 255;
				((uint8_t*)tmp)[i] = c;
			}

			for (int i = FRAME_H; i--;)
			{
				memcpy(rgb_buf + (i * FRAME_W), tmp + ((FRAME_H - i) * FRAME_W), sizeof(color_t) * FRAME_W);
			}

			rgb_to_yuv422(state.view.luma, state.view.chroma, rgb_buf, FRAME_W, FRAME_H);

			msg.payload.state = state;
			if (write_pipeline_payload(&msg))
			{
				return -1;
			}
		}

		renderer.draw(&camera, { &sky_pass, &ground_pass, &bale_pass });

		// sleep(1);
		// usleep(1000000);
	}

	// close(sock_fd);
	unlink("./avc.sim.ctrl");
	kill(0, 9);

	return 0;
}
