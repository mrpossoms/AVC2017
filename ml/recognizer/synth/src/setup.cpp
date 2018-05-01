int w = 512, h = 512;

if (argc > 2)
{
	w = 64;
	h = 64;
}

srand(time(NULL));

seen::RendererGL renderer("./data/", argv[0], w, h);
seen::ListScene scene;
seen::Camera camera(M_PI / 4, renderer.width, renderer.height);

seen::ShaderConfig disp_shader = {
	.vertex = "displacement.vsh",
	.tessalation = {
		.control = "displacement.tcs",
		.evaluation = "displacement.tes",
	},
	.geometry = "",
	.fragment = "basic.fsh"
};
