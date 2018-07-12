
class Asphalt : public seen::Drawable {
	mat4x4_t _world;
	mat3x3_t _rot;
	float _tex_rotation;
public:
	seen::Model* model;
	seen::Material* mat;
	seen::Tex disp_tex;
	seen::Tex paint_tex;

	Asphalt()
	{
		model = seen::MeshFactory::get_model("asphalt.obj");
		mat = seen::TextureFactory::get_material("asphalt");
		disp_tex = seen::TextureFactory::load_texture("asphalt.displacement.png");
		paint_tex = seen::TextureFactory::load_texture("parking.color.png");

		_tex_rotation = seen::rf() * M_PI;

		mat4x4_identity(_world.v);

		for(int i = 9; i--;)
		{
			_rot.v[i % 3][i / 3] = _world.v[i % 3][i / 3];
		}

		mat4x4_scale(_world.v, _world.v, 10);
	}

	void draw(seen::Viewer* viewer)
	{
		assert(seen::gl_get_error());

		vec3_t tex_control = { 0, 0, 500 };
		seen::ShaderProgram& shader = *seen::ShaderProgram::active();

		shader["u_normal_matrix"] << _rot;
		shader["u_world_matrix"] << _world;
		shader["u_tex_control"] << tex_control;
		shader["u_texcoord_rotation"] << _tex_rotation;
		assert(seen::gl_get_error());

		model->draw(viewer);
		assert(seen::gl_get_error());
	}
};

class HayBale : public seen::Drawable {
	seen::Model* _model;
public:
	mat4x4_t world;
	float disp_weight;
	HayBale()
	{
		_model = seen::MeshFactory::get_model("cube.obj");
		disp_weight = seen::rf(0.1, 0.5);
	}

	void draw(seen::Viewer* viewer)
	{
		assert(seen::gl_get_error());
		mat3x3_t _rot;
		for(int i = 9; i--;)
		{
			_rot.v[i % 3][i / 3] = world.v[i % 3][i / 3];
		}

		vec3_t tex_control = { 0, 0, 2 };
		seen::ShaderProgram& shader = *seen::ShaderProgram::active();

		shader["u_normal_matrix"] << _rot;
		shader["u_world_matrix"] << world;
		shader["u_displacement_weight"] << disp_weight;
		shader["u_tex_control"] << tex_control;
		_model->draw(viewer);
		assert(seen::gl_get_error());
	}

	static seen::Material* material()
	{
		static seen::Material* mat;

		if (!mat)
		{
			mat = seen::TextureFactory::get_material("hay");
		}

		return mat;
	}

	static seen::Tex displacement_tex()
	{
		static seen::Tex disp_tex;

		if (!disp_tex)
		{
			disp_tex = seen::TextureFactory::load_texture("hay.displacement.png");
		}

		return disp_tex;
	}
};
