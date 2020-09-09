#version 400 core
#define COOK_BLINN
#define COOK
#define USE_ALBEDO_MAP
// #define USE_NORMAL_MAP
// #define USE_ROUGHNESS_MAP

in vec2 v_texcoord; // texture coords
in vec3 v_normal;   // normal
in vec3 v_binormal; // binormal (for TBN basis calc)
in vec3 v_pos;      // pixel view space position
in vec4 v_screen_space;      // pixel view space position


out vec4 color;


uniform mat4x4 world_matrix;  // object's world position
uniform mat4x4 view_matrix;   // view (camera) transform
uniform mat4x4 proj_matrix;   // projection matrix
uniform mat3x3 normal_matrix; // normal transformation matrix ( transpose(inverse(W * V)) )


uniform vec4 material; // x - metallic, y - roughness, w - "rim" lighting
uniform vec4 albedo;   // constant albedo color, used when textures are off


uniform sampler2D tex;     // base texture (albedo)
uniform sampler2D norm;    // normal map
uniform sampler2D spec;    // "factors" texture (G channel used as roughness)
uniform sampler2D iblbrdf; // IBL BRDF normalization precalculated tex
uniform samplerCube envd;  // prefiltered env cubemap

#define PI 3.1415926

const vec3 light_dir = normalize(vec3(0.0, 1.0, 1.0));

vec3 kernel[] = vec3[](
	vec3(0.779529, 0.623008, -0.064770),
	vec3(0.129716, -0.772926, -0.621095),
	vec3(-0.640137, 0.692341, 0.333000),
	vec3(-0.601690, -0.721712, -0.342200),
	vec3(0.696391, 0.659027, 0.284119),
	vec3(0.651141, 0.605876, 0.457088),
	vec3(0.552877, -0.573648, -0.604363),
	vec3(-0.185592, -0.639798, 0.745798),
	vec3(-0.495773, 0.800052, 0.337826),
	vec3(-0.311609, -0.621665, -0.718632),
	vec3(0.740552, -0.151572, 0.654682),
	vec3(-0.564898, -0.789267, -0.240725),
	vec3(0.928075, 0.285612, 0.238959),
	vec3(-0.010215, -0.398410, 0.917151),
	vec3(0.274089, 0.703027, 0.656223),
	vec3(-0.097895, -0.733350, -0.672766),
	vec3(-0.793562, -0.597204, -0.116649),
	vec3(0.059294, 0.663230, -0.746063),
	vec3(-0.128960, 0.989179, 0.069960),
	vec3(0.188737, 0.886407, -0.422684),
	vec3(-0.842768, 0.533782, 0.069419),
	vec3(0.702450, 0.215861, 0.678210),
	vec3(0.673806, -0.581471, -0.455936),
	vec3(-0.721687, 0.669206, -0.177006),
	vec3(-0.339596, -0.833807, 0.435247),
	vec3(0.620169, -0.513216, -0.593296),
	vec3(0.665434, 0.516676, 0.538743),
	vec3(0.781294, -0.071142, 0.620095),
	vec3(0.598897, -0.133905, 0.789551),
	vec3(0.355762, -0.904827, -0.233928),
	vec3(0.670036, -0.093307, 0.736441),
	vec3(-0.657081, 0.716457, -0.234379)
);

// float chebyshevUpperBound(vec4 ShadowCoordPostW, float distance)
// {
// 	// We retrive the two moments previously stored (depth and depth*depth)
// 	vec2 moments = texture2D(ShadowMap,ShadowCoordPostW.xy).rg;
//
// 	// Surface is fully lit. as the current fragment is before the light occluder
// 	if (distance <= moments.x)
// 		return 1.0 ;
//
// 	// The fragment is either in shadow or penumbra. We now use chebyshev's upperBound to check
// 	// How likely this pixel is to be lit (p_max)
// 	float variance = moments.y - (moments.x*moments.x);
// 	variance = max(variance,0.00002);
//
// 	float d = distance - moments.x;
// 	float p_max = variance / (variance + d*d);
//
// 	return p_max;
// }

void main() {
    vec3 nn = normalize(v_normal + v_pos);

    vec3 nb = normalize(cross(nn, v_binormal));
    mat3x3 tbn = mat3x3(nb, cross(nn, nb), nn);

    vec2 texcoord = v_texcoord;



    vec3 mapped_normal = texture(norm, texcoord).xyz * 2.0 - 1.0;
    vec3 N = tbn * mapped_normal;


	vec3 ibl = vec3(0.0);

	// for(int i = 0; i < 16; ++i)
	// {
	// 	ibl += texture(envd, N + kernel[i] * 0.5).xyz;
	// }
	//
	// ibl /= 16.0;

	vec3 base_color = texture(tex, texcoord).xyz;
	vec3 result = base_color * ibl * 0.9 + 0.1;

	color = vec4(result, 1.0);
}
