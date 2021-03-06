#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "seen.hpp"
#include "sky.hpp"
#include "json.hpp"

#include "sys.h"

using namespace nlohmann;

#include "drawables.hpp"
#include "helpers.hpp"

#define DEG_2_RAD(deg) (((deg) * M_PI) / 180.f)

static vec3_t one = { 1, 1, 1 };

static vec4_t material = { 0.1, 0.01, 1, 0.01 };
static vec3_t light_dir = { 1, -1, 1 };
static vec3_t tex_control = { 0, 0, 16 };

bool PAUSED = false;
bool STEER_LOCKED = false;
bool DO_STEREO_CAPTURE = false;

const float IPD = 0.05;

struct {
	Vec3 position = { 0, -0.5, 0 };
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

	Vec3 left()
	{
		Vec3 dir(cos(angle - M_PI), 0, sin(angle - M_PI));
		return dir;
	}

	float steer_speed()
	{
		return steering_angle * speed;
	}

	void turn(float d_theta)
	{
		const float dwell = 0.5;
		steering_angle = (1.f - dwell) * d_theta + dwell * steering_angle;
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

	seen::RendererGL renderer("./data", "Sim", FRAME_W >> 1, FRAME_H >> 1, 4, 0);
	seen::Camera camera(DEG_2_RAD(62.2), renderer.width, renderer.height);

	seen::ListScene sky_scene, ground_scene, hay_scene, shadow_scene;

	json scene_json;
	i >> scene_json;

	// Sky setup
	seen::Model* sky = seen::MeshFactory::get_model("sphere.obj");
	seen::CustomPass sky_pass([&](int index) {
		seen::ShaderProgram::builtin_sky().use();
		glDisable(GL_CULL_FACE);
	});

	sky_scene = { sky };
	sky_pass.scene = &sky_scene;

	// Asphalt setup
	Asphalt asphalt;
	HayBale::material();

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
	   .shadow_mapped_vsm()
	   .blinn()
	;

	seen::ShaderProgram& primary_shader = seen::ShaderProgram::compile("primary", { vsh, fsh });
	const float light_power = 40;
	seen::Light light = {
		.position = { 0, 30, 0 },
		.power = { light_power, light_power, light_power },
		.ambience = 0.01
	};
	mat4x4_perspective(light.projection.v, M_PI / 2, 1, 0.1, 100);


	auto shadow_pass = seen::ShadowPass(512, 1);
	seen::CustomPass ground_pass([&](int index) {
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
		primary_shader.use();


		primary_shader["u_view_position"] << camera.position();
		primary_shader << &light;
		primary_shader << &shadow_pass;
		primary_shader << asphalt.mat;

	});
	ground_scene.insert(&asphalt);
	ground_pass.scene = &ground_scene;

	seen::CustomPass bale_pass([&](int index) {
		// ShaderConfig shader_desc = SURFACE_SHADER;
		// ShaderProgram& shader = *Shaders[shader_desc]->use();
		//
		// shader["TessLevelInner"] << 5.0f;
		// shader["TessLevelOuter"] << 5.0f;
		// shader["us_displacement"] << HayBale::displacement_tex();
		// primary_shader->use();
		primary_shader << HayBale::material();
	});
	bale_pass.scene = &hay_scene;

	// load all the haybales in the scene
	mat4x4_t I;
	mat4x4_identity(I.v);
	mat4x4_translate_in_place(I.v, 0, 0, 0);
	auto root = scene_json["object"];
	populate_scene(hay_scene, root, I);

	shadow_scene.insert(&ground_scene);
	shadow_scene.insert(&hay_scene);
	shadow_pass.scene = &shadow_scene;
	shadow_pass.lights.push_back(&light);

	// camera.position(0, 0, 0);
	int sock_fd = open_ctrl_pipe();
	bool turn_key_pressed;

	renderer.key_pressed = [&](int key) {
		switch (key) {
			case GLFW_KEY_L:
				STEER_LOCKED = true;
				break;
			case GLFW_KEY_LEFT:
				vehicle.turn(-0.2);
				turn_key_pressed = true;
				break;
			case GLFW_KEY_RIGHT:
				vehicle.turn(0.2);
				turn_key_pressed = true;
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
			case GLFW_KEY_S:
				DO_STEREO_CAPTURE = true;
				break;
		}
	};

	message_t msg = {
		.header = {
			.magic = MAGIC,
			.type  = PAYLOAD_STATE
		},
	};

	float t = 0;
	while (renderer.is_running())
	{
		// look for socket input
		poll_ctrl_pipe(sock_fd);

		// simulate high frequency tilting of the platform as road noise
		Quat q = vehicle.orientation();
		Quat tilt, roll, jitter;
		Vec3 ja = seen::rn(); // jitter axis
		quat_from_axis_angle(tilt.v, 1, 0, 0, 0.35);
		quat_from_axis_angle(roll.v, 0, 0, 1, -1.0f * vehicle.speed * vehicle.steer_speed());
		quat_from_axis_angle(jitter.v, ja.x, ja.y, ja.z, seen::rf(-0.01, 0.01) * vehicle.speed);

		if (!PAUSED)
		{
			if (sock_fd < 0 && !turn_key_pressed)
			{
				vehicle.turn(0);
			}

			vehicle.update();
			q = tilt * roll * jitter * q;
			camera.orientation(q);
			camera.position(vehicle.position);

			Vec3 heading = vehicle.heading();

			raw_state_t state = {
				.rot_rate = {},
				.acc = {},
			        .vel = vehicle.speed,
				.distance = vehicle.distance,
				.heading = { heading.x, heading.z, heading.y },
				.position = { vehicle.position.x, vehicle.position.z, vehicle.position.y }
			};

			std::vector<seen::RenderingPass*> passes = { &shadow_pass, &sky_pass, &ground_pass, &bale_pass };

			if (DO_STEREO_CAPTURE)
			{
				Vec3 old_pos = camera.position();
				Vec3 offsets[] = {
					vehicle.left() * IPD,
					vehicle.left() * -IPD
				};
				const std::string names[] = { "l.png", "r.png" };

				char hash[16];
				sprintf(hash, "%lx_", random());

				for (int i = 2; i--;)
				{
					Vec3 offset = old_pos + offsets[i];
					camera.position(offset);
					renderer.draw(&camera, passes);
					renderer.capture("./" + std::string(hash) + names[i]);
				}

				DO_STEREO_CAPTURE = false;
			}
			else
			{
				renderer.draw(&camera, passes);
			}

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

			// flip image vertically
			for (int i = FRAME_H; i--;)
			{
				memcpy(rgb_buf + (i * FRAME_W), tmp + (((FRAME_H - 1) - i) * FRAME_W), sizeof(color_t) * FRAME_W);
			}

			rgb_to_yuv422(state.view.luma, state.view.chroma, rgb_buf, FRAME_W, FRAME_H);

			msg.payload.state = state;
			if (write_pipeline_payload(&msg))
			{
				return -1;
			}
		}

		light.position.x = cos(t) * 10;
		light.position.z = sin(t) * 10;
		t += 0.01f;

		turn_key_pressed = false;

		renderer.draw(&camera, {
			&shadow_pass,
			&sky_pass,
			&ground_pass,
			&bale_pass
		});

		// sleep(1);
		// usleep(1000000);
	}

	// close(sock_fd);
	unlink("./avc.sim.ctrl");
	kill(0, 9);

	return 0;
}
