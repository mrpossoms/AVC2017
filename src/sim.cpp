#include "seen.hpp"
#include "sky.hpp"

#define SURFACE_SHADER {                  \
	.vertex = "displacement.vsh",         \
	.tessalation = {                      \
		.control = "displacement.tcs",    \
		.evaluation = "displacement.tes", \
	},                                    \
	.geometry = "",                       \
	.fragment = "basic.fsh" }             \


using namespace seen;

static vec3_t one = { 1, 1, 1 };

static vec4_t material = { 0.1, 0.01, 1, 0.01 };
static vec3_t light_dir = { 1, -1, 1 };
static vec3_t tex_control = { 0, 0, 16 };


class Asphalt : public Drawable {
private:
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

	renderer.key_pressed = [&](int key) {
		Quat q = camera.orientation();
		Quat delta;

		switch (key) {
			case GLFW_KEY_LEFT:
				quat_from_axis_angle(delta.v, 0, 1, 0, -0.1);
				break;
			case GLFW_KEY_RIGHT:
				quat_from_axis_angle(delta.v, 0, 1, 0, 0.1);
				break;
		}


		q = delta * q;

		camera.orientation(q);
	};

	while (renderer.is_running())
	{
		renderer.draw(&camera, &scene);
	}

	return 0;
}
