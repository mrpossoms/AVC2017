#version 400 core

in vec2 te_texcoord; // texture coords
in vec3 te_normal;   // normal
in vec3 te_tangent;  // tangent
in vec3 te_binormal; // binormal (for TBN basis calc)

out vec4 color;

uniform sampler2D us_color;     // base texture (albedo)
uniform sampler2D us_normal;
uniform sampler2D us_displacement;

uniform vec3 u_light_dir;
uniform vec3 u_tint;

void main()
{
    vec3 rgb = texture(us_color, te_texcoord).xyz * u_tint;
    vec3 tn = normalize((texture(us_normal, te_texcoord).xyz * 2.0) - 1.0);
    mat3 tbn = mat3(te_binormal, te_tangent, te_normal);

    vec3 normal = tbn * tn;
    //vec3 normal = tbn * vec3(0.0, 0.0, 1.0);

    float shade = (dot(normalize(-u_light_dir), normal) + 1.0) / 2.0;
    //float shade = (dot(normalize(-u_light_dir), te_normal) + 1.0) / 2.0;
    //shade *= (dot(normalize(-u_light_dir), normal) + 1.0) / 2.0;
    float l = shade * 1.0 + 0.25;
	// color = vec4(l, 0.0, 0.0, 1.0);
    // color = vec4(vec3(v_texcoord, 1.0), 1.0);
    color = vec4(rgb.xyz * l, 1.0);
    //color = vec4(vec3(l), 1.0);
    // color = vec4(l * ((normal.xyz / 2.0) + 0.5), 1.0);
    // color = vec4(((normal.xyz / 2.0) + 0.5), 1.0);
}
