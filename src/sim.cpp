#include "seen.hpp"
#include "sky.hpp"
#include "json.hpp"

#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SURFACE_SHADER {                      \
	.vertex = "displacement.vsh",             \
	.tessalation = {                          \
		.control = "displacement.tcs",        \
		.evaluation = "displacement.tes",     \
	},                                        \
	.geometry = "",                           \
	.fragment = "basic.fsh" }                 \


using namespace seen;
using namespace nlohmann;

static vec3_t one = { 1, 1, 1 };

static vec4_t material = { 0.1, 0.01, 1, 0.01 };
static vec3_t light_dir = { 1, -1, 1 };
static vec3_t tex_control = { 0, 0, 16 };


class Asphalt : public Drawable {
	mat4x4_t _world;
	mat3x3_t _rot;
public:
	seen::Model* model;
	seen::Material* mat;
	seen::Tex disp_tex;

	Asphalt()
	{
		model = seen::MeshFactory::get_model("asphalt.obj");
		mat = seen::TextureFactory::get_material("asphalt");
		disp_tex = seen::TextureFactory::load_texture("asphalt.displacement.png");

		mat4x4_identity(_world.v);

		for(int i = 9; i--;)
		{
			_rot.v[i % 3][i / 3] = _world.v[i % 3][i / 3];
		}

		mat4x4_scale(_world.v, _world.v, 100);
	}

	void draw(Viewer* viewer)
	{
		assert(gl_get_error());

		vec3_t tex_control = { 0, 0, 1000 };
		ShaderProgram& shader = *ShaderProgram::active();

		shader["u_normal_matrix"] << _rot;
		shader["u_world_matrix"] << _world;
		shader["u_tex_control"] << tex_control;
		assert(gl_get_error());

		model->draw(viewer);
		assert(gl_get_error());
	}
};

class HayBale : public Drawable {
	Model* _model;
public:
	mat4x4_t world;
	float disp_weight;
	HayBale()
	{
		_model = MeshFactory::get_model("cube.obj");
		disp_weight = seen::rf(0.1, 0.5);
	}

	void draw(Viewer* viewer)
	{
		assert(gl_get_error());
		mat3x3_t _rot;
		for(int i = 9; i--;)
		{
			_rot.v[i % 3][i / 3] = world.v[i % 3][i / 3];
		}

		vec3_t tex_control = { 0, 0, 2 };
		ShaderProgram& shader = *ShaderProgram::active();

		shader["u_normal_matrix"] << _rot;
		shader["u_world_matrix"] << world;
		shader["u_displacement_weight"] << disp_weight;
		shader["u_tex_control"] << tex_control;
		_model->draw(viewer);
		assert(gl_get_error());
	}

	static Material* material()
	{
		static Material* mat;

		if (!mat)
		{
			mat = TextureFactory::get_material("hay");
		}

		return mat;
	}

	static Tex displacement_tex()
	{
		static Tex disp_tex;

		if (!disp_tex)
		{
			disp_tex = TextureFactory::load_texture("hay.displacement.png");
		}

		return disp_tex;
	}
};


mat4x4_t mat_from_json(json& obj)
{
	mat4x4_t tmp, M;
	auto matrix = obj["matrix"];
	for (int i = 16; i--;)
	{
		tmp.v[i % 4][i / 4] = matrix[i];
	}

	mat4x4_transpose(M.v, tmp.v);

	return M;
}

void populate_scene(CustomPass& pass, json& obj, mat4x4_t world)
{
	mat4x4_t tmp, my_world;

	tmp = mat_from_json(obj);
	mat4x4_mul(my_world.v, world.v, tmp.v);

	for (auto child : obj["children"])
	{
		if (child["type"] == "Mesh")
		{
			mat4x4_t child_mat = mat_from_json(child);
			mat4x4_t purturbed;


			mat4x4_rotate(purturbed.v, child_mat.v, 0, 1, 0, seen::rf(-0.1, 0.1));

			auto bale = new HayBale();
			mat4x4_mul(bale->world.v,  my_world.v, purturbed.v);
			// mat4x4_transpose(bale->world.v, tmp.v);
			pass.drawables.push_back(bale);
		}
		else {
			populate_scene(pass, child, my_world);
		}
	}
}


int open_ctrl_socket()
{
	struct sockaddr_un namesock = {};
	int fd;
	namesock.sun_family = AF_UNIX;

	strncpy(namesock.sun_path, "/tmp/avc.sim.ctrl", sizeof(namesock.sun_path));

	fd = socket(AF_UNIX, SOCK_DGRAM, 0);

	if (fd < 0)
	{
		return -1;
	}

	if (bind(fd, (struct sockaddr *) &namesock, sizeof(struct sockaddr_un)))
	{
		return -2;
	}

	return fd;
}


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

	RendererGL renderer("./data", "Sim");
	Camera camera(M_PI / 2, renderer.width, renderer.height);

	ListScene scene;

	json scene_json;
	i >> scene_json;

	glEnable(GL_CULL_FACE);

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
		shader["TessLevelInner"] << 1.0f;
		shader["TessLevelOuter"] << 1.0f;
		shader["u_tint"] << one;

		shader["us_displacement"] << asphalt.disp_tex;
		shader << asphalt.mat;
	}, &asphalt, NULL);

	CustomPass bale_pass([&]() {
		ShaderConfig shader_desc = SURFACE_SHADER;
		ShaderProgram& shader = *Shaders[shader_desc]->use();

		shader["u_displacement_weight"] << 0.1f;
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

	camera.position(0, -1, 0);
	scene.drawables().push_back(&sky_pass);
	scene.drawables().push_back(&ground_pass);
	scene.drawables().push_back(&bale_pass);

	int sock_fd = open_ctrl_socket();

	float t = 0;
	renderer.key_pressed = [&](int key) {
		Quat q = camera.orientation();
		Quat delta;
		float speed = 0;
		float turn_speed = 0.1;

		switch (key) {
			case GLFW_KEY_LEFT:
				quat_from_axis_angle(delta.v, 0, 1, 0, -turn_speed);
				t += -turn_speed;
				break;
			case GLFW_KEY_RIGHT:
				quat_from_axis_angle(delta.v, 0, 1, 0, turn_speed);
				t += turn_speed;
				break;
			case GLFW_KEY_UP:
			{
				speed = 0.1;
			}
				break;
			case GLFW_KEY_DOWN:
			{
				speed = -0.1;
			}
				break;
		}

		Vec3 pos = camera.position();
		Vec3 heading(cos(t + M_PI / 2) * speed, 0, sin(t + M_PI / 2) * speed);
		pos = pos + heading;
		camera.position(pos);

		q = delta * q;
		camera.orientation(q);
	};

	while (renderer.is_running())
	{
		// look for socket input
		poll_ctrl_sock(sock_fd);

		renderer.draw(&camera, &scene);
	}

	close(sock_fd);
	unlink("/tmp/avc.sim.ctrl");

	return 0;
}
