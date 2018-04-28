#include "seen.hpp"
#include "sky.hpp"
#include "json.hpp"

#define SURFACE_SHADER {                          \
	.vertex = "displacement.vsh",             \
	.tessalation = {                          \
		.control = "displacement.tcs",    \
		.evaluation = "displacement.tes", \
	},                                        \
	.geometry = "",                           \
	.fragment = "basic.fsh" }                 \


using namespace seen;
using namespace nlohmann;

static vec3_t one = { 1, 1, 1 };

static vec4_t material = { 0.1, 0.01, 1, 0.01 };
static vec3_t light_dir = { 1, -1, 1 };
static vec3_t tex_control = { 0, 0, 16 };


void printm(mat4x4_t m)
{
	for (int r = 0; r < 4; ++r)
	{
		for (int c = 0; c < 4; ++c)
		{
			if (c == 0) printf("| ");
			printf("%0.3f ", m.v[r][c]);
			if (c == 3) printf("|");
		}
		printf("\n");
	}
}


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
		vec3_t tex_control = { 0, 0, 1000 };
		ShaderProgram& shader = *ShaderProgram::active();
		shader["u_normal_matrix"] << _rot;
		shader["u_world_matrix"] << _world;

		shader["u_tex_control"] << tex_control;
		shader["us_displacement"] << disp_tex;

		shader << mat;

		model->draw(viewer);
	}
};

class HayBale : public Drawable {
	Model* _model;
	Material* _mat;
	Tex _disp_tex;
public:
	mat4x4_t world;

	HayBale()
	{
		_model = MeshFactory::get_model("cube.obj");
		_mat = TextureFactory::get_material("hay");
		_disp_tex = TextureFactory::load_texture("hay.displacement.png");
	}

	void draw(Viewer* viewer)
	{
		mat3x3_t _rot;
		for(int i = 9; i--;)
		{
			_rot.v[i % 3][i / 3] = world.v[i % 3][i / 3];
		}

		vec3_t tex_control = { 0, 0, 2 };
		ShaderProgram& shader = *ShaderProgram::active();
		shader["u_normal_matrix"] << _rot;
		auto param = shader["u_world_matrix"];
		param << world;

		shader["u_displacement_weight"] << 0.25f;
		shader["u_tex_control"] << tex_control;
		shader["us_displacement"] << _disp_tex;

		shader << _mat;

		_model->draw(viewer);
	}
};


mat4x4_t mat_from_json(json& obj)
{
	mat4x4_t tmp;
	auto matrix = obj["matrix"];
	for (int i = 16; i--;)
	{
		tmp.v[i % 4][i / 4] = matrix[i];
	}

	return tmp;
}

void populate_scene(CustomPass& pass, json& obj, mat4x4_t world)
{
	for (auto child : obj["children"])
	{
		mat4x4_t tmp, my_world;

		tmp = mat_from_json(obj);
		mat4x4_mul(my_world.v, tmp.v, world.v);

		if (child["type"] == "Mesh")
		{
			mat4x4_t child_mat = mat_from_json(child);
			mat4x4_mul(tmp.v, child_mat.v, my_world.v);

			auto bale = new HayBale();
			mat4x4_transpose(bale->world.v, tmp.v);
			pass.drawables.push_back(bale);
		}
		else {
			populate_scene(pass, child, my_world);
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

	// Sky setup
	Sky sky;
	CustomPass sky_pass([&]() {
		// draw pass preparation
		ShaderConfig shader_desc = SKY_SHADERS;
		ShaderProgram* shader = Shaders[shader_desc]->use();
	}, &sky, NULL);


	// Asphalt setup
	Asphalt asphalt;

	CustomPass surface_pass([&]() {
		// draw pass preparation
		ShaderConfig shader_desc = SURFACE_SHADER;
		ShaderProgram& shader = *Shaders[shader_desc]->use();

		shader["u_light_dir"] << light_dir;
		shader["u_displacement_weight"] << 0.1f;
		shader["u_tex_control"] << tex_control;
		shader["TessLevelInner"] << 1.0f;
		shader["TessLevelOuter"] << 1.0f;
		shader["u_tint"] << one;

	}, &asphalt, NULL);

	mat4x4_t I;
	mat4x4_identity(I.v);
	auto root = scene_json["object"];
	populate_scene(surface_pass, root, I);

	camera.position(0, -1, 0);
	scene.drawables().push_back(&sky_pass);
	scene.drawables().push_back(&surface_pass);

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
		renderer.draw(&camera, &scene);
	}

	return 0;
}
