#include <seen/seen.hpp>
#include <png.h>
#include <sstream>

static vec3_t lerp(const vec3_t v[2], float p)
{
	float p1 = 1 - p;
	vec3_t o = {
		v[0].x * p1 + v[1].x * p,
		v[0].y * p1 + v[1].y * p,
		v[0].z * p1 + v[1].z * p,
	};

	return o;
}

int main(int argc, const char* argv[])
{
	#include "setup.cpp"

	seen::Model* bale = seen::MeshFactory::get_model("cube.obj");
	seen::Material* bale_mat = seen::TextureFactory::get_material("hay");

	std::string disp_name = "hay.displacement.png";

	seen::Tex displacement_tex = seen::TextureFactory::load_texture(disp_name);
	seen::CustomPass bale_pass, bale_tess_pass;
	Quat q_bale_ori;
	vec3_t light_dir = { -1, 0, rand() % 2 ? -1.0f : 1.0f };
	vec3_t tint;



	camera.position(0, 0, -3);

	// callback for camera looking
	renderer.mouse_moved = [&](double x, double y, double dx, double dy)
	{
		Quat pitch, yaw, roll;
		Vec3 forward, left, up;

		pitch.from_axis_angle(VEC3_LEFT.v[0], VEC3_LEFT.v[1], VEC3_LEFT.v[2], dy * 0.01);
		yaw.from_axis_angle(VEC3_UP.v[0], VEC3_UP.v[1], VEC3_UP.v[2], dx * 0.01);
		pitch = pitch * yaw;
		q_bale_ori = pitch * q_bale_ori;
	};

	float uv_rot = 0;
	bale_pass.preparation_function = [&]()
	{
		vec4_t material = { 0.1, 0.01, 1, 0.01 };
		vec4_t albedo = { 1, 1, 1, 1 };
		mat4x4_t world;
		mat3x3_t rot;
		vec3 axis = { 0.0, 1.0, 0.0 };
		vec3_t tex_control = { 0, 0, 4 };
		const vec3_t tints[2] = {
			{ 229 / 255.f, 203 / 255.f, 181 / 255.f },
			{ 1, 1.1, 1.1 }
		};

		// glClear(GL_DEPTH_BUFFER_BIT);

		if (argc > 2)
		{
			Quat pitch;

			uv_rot = seen::rf(0, 2 * M_PI);
			tint = lerp(tints, seen::rf());

			vec3_norm(axis, axis);
			quat_from_axis_angle(q_bale_ori.v, axis[0], axis[1], axis[2], seen::rf(0, 2 * M_PI));
			pitch.from_axis_angle(VEC3_LEFT.v[0], VEC3_LEFT.v[1], VEC3_LEFT.v[2], seen::rf(-1, 1) * 0.01);
			
			q_bale_ori = pitch * q_bale_ori;
			
			camera.fov(M_PI / seen::rf(4,4));

			tex_control.x = seen::rf(-1, 1);
			tex_control.y = seen::rf(-1, 1);
			tex_control.z = seen::rf(0.25, 3);
		}
		else
		{
			uv_rot += 0.0001f;
			tint = tints[1];
		}

		seen::ShaderProgram& shader = *seen::Shaders[disp_shader]->use();

		shader << bale_mat; //->use(&shader.draw_params.material_uniforms.tex);

		mat4x4_from_quat(world.v, q_bale_ori.v);

		if (argc > 2)
		{
			mat4x4_translate_in_place(world.v, seen::rf(-0.1, 0.1), seen::rf(-0.1, 0.1), seen::rf(0.1, 0.1));
		}

		for(int i = 3; i--;)
		for(int j = 3; j--;)
		{
			rot.v[i][j] = world.v[i][j];
		}

		shader["u_world_matrix"] << world;
		shader["u_normal_matrix"] << rot;

		vec3_t inner_tint;
		vec3_scale(inner_tint.v, tint.v, 0.75f);

		shader["u_light_dir"] << light_dir;
		shader["u_texcoord_rotation"] << uv_rot;
		shader["u_displacement_weight"] << 0.0f;
		shader["u_tex_control"] << tex_control;
		shader["TessLevelInner"] << 1.0f;
		shader["TessLevelOuter"] << 1.0f;
		shader["u_tint"] << inner_tint;
		// glDisable(GL_CULL_FACE);
	};

	bale_tess_pass.preparation_function = [&]()
	{
		seen::ShaderProgram& shader = *seen::Shaders[disp_shader]->use();

		shader["us_displacement"] << displacement_tex;

		shader["u_displacement_weight"] << 0.125f;
		shader["TessLevelInner"] << 5.0f;
		shader["TessLevelOuter"] << 13.0f;
		shader["u_tint"] << tint;
	};

	// Add all things to be drawn to the scene
	bale_pass.drawables = new std::vector<seen::Drawable*>();
	bale_pass.drawables->push_back(bale);

	bale_tess_pass.drawables = new std::vector<seen::Drawable*>();
	bale_tess_pass.drawables->push_back(bale);

	for(int i = 1; i--;)
	{
		scene.drawables().push_back(&bale_pass);
		scene.drawables().push_back(&bale_tess_pass);
	}

	renderer.clear_color(seen::rf(), seen::rf(), seen::rf(), 1);

	int i = argc >= 3 ? atoi(argv[2]) : 10e6;
	for(; renderer.is_running() && i--;)
	{
		if (argc > 2)
		{
			light_dir.x = seen::rf(-1, 1);
			light_dir.z = rand() % 3 == 0 ? 1.0f : -1.0f;
		}

		renderer.draw(&camera, &scene);

		if (argc > 2)
		{
			std::stringstream path_ss;
			path_ss << argv[1] << "/" << std::hex << random();
			renderer.capture(path_ss.str());

			renderer.clear_color(seen::rf(), seen::rf(), seen::rf(), 1);
		}
	}

	return 0;
}
