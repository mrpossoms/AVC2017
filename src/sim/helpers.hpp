#define SURFACE_SHADER {                          \
	.vertex = "displacement.vsh",             \
	.fragment = "basic.fsh",                  \
	.tessalation = {                          \
		.control = "displacement.tcs",    \
		.evaluation = "displacement.tes"  \
	},                                        \
	.geometry = "" }                          \


#define OVERLAY_SURFACE_SHADER {                  \
	.vertex = "displacement.vsh",             \
	.fragment = "basic_overlay.fsh",          \
	.tessalation = {                          \
		.control = "displacement.tcs",    \
		.evaluation = "displacement.tes", \
	},                                        \
	.geometry = "",                           \
	}                                         \

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

void populate_scene(seen::Scene& scene, json& obj, mat4x4_t world)
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
			mat4x4_mul(tmp.v,  my_world.v, purturbed.v);
			bale->world(tmp.v);
			// mat4x4_transpose(bale->world.v, tmp.v);
			scene.insert(bale);
		}
		else {
			populate_scene(scene, child, my_world);
		}
	}
}


int open_ctrl_pipe()
{
	const char* path = "./avc.sim.ctrl";
	mkfifo(path, 0666);

	int fd = open(path, O_RDONLY | O_NONBLOCK);

	if (fd < 0)
	{
		return -1;
	}

	return fd;
}


void rgb_to_yuv422(uint8_t* luma, chroma_t* uv, color_t* rgb, int w, int h)
{
	for(int yi = h; yi--;)
	for(int xi = w; xi--;)
	{
		int i = yi * w + xi;
		int j = yi * (w >> 1) + (xi >> 1);

		int l = (rgb[i].r + rgb[i].g + rgb[i].b) / 3;

		luma[i] = l;
		uv[j].cb = ((rgb[i].r - l) / 1.14f) - 128;
		uv[j].cr = ((rgb[i].b - l) / 2.033f) - 128;
	}
}
