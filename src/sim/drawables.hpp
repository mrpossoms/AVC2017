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

		mat4x4_scale(_world.v, _world.v, 10);
	}

	void draw(Viewer* viewer)
	{
		assert(gl_get_error());

		vec3_t tex_control = { 0, 0, 500 };
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
