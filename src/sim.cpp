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
		vec3_t tex_control = { 0, 0, 1000 };
		ShaderProgram& shader = *ShaderProgram::active();
		shader["u_normal_matrix"] << _rot;
		shader["u_world_matrix"] << _world;

		shader["u_tex_control"] << tex_control;

		shader << mat;

		model->draw(viewer);
	}
};

class HayBale : public Drawable {
	Model* _model;
	Material* _mat;
	Tex _disp_tex;
	mat4x4_t _world;

public:
	HayBale()
	{
		_model = MeshFactory::get_model("spherized_cube.obj");
		_mat = TextureFactory::get_material("hay");
		_disp_tex = TextureFactory::load_texture("hay.displacement.png");
	}

	void draw(Viewer* viewer)
	{
		mat3x3_t _rot;
		for(int i = 9; i--;)
		{
			_rot.v[i % 3][i / 3] = _world.v[i % 3][i / 3];
		}

		vec3_t tex_control = { 0, 0, 2 };
		ShaderProgram& shader = *ShaderProgram::active();
		shader["u_normal_matrix"] << _rot;
		shader["u_world_matrix"] << _world;

		shader["u_tex_control"] << tex_control;

		shader << _mat;

		_model->draw(viewer);
	}	
};

int main (int argc, char* argv[])
{
	RendererGL renderer("./data", "Sim");
	Camera camera(M_PI / 2, renderer.width, renderer.height);

	ListScene scene;

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
