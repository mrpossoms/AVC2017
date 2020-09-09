#version 330 core

in vec2 v_texcoord; // texture coords
in vec3 v_normal;   // normal
in vec3 v_binormal; // binormal (for TBN basis calc)
in vec3 v_pos;      // pixel view space position

out vec4 color;

const vec3 light_dir = normalize(vec3(0.0, 1.0, 1.0));
const vec3 dark_blue = vec3(4.0 / 255.0, 39.0 / 255.0, 181.0 / 255.0);
const vec3 light_blue = vec3(131.0 / 255.0, 187.0 / 255.0, 248.0 / 255.0);
const vec3 sun_color = vec3(253.0 / 255.0, 184.0 / 255.0, 19.0 / 255.0);

void main()
{
    vec3 norm = normalize(v_pos);
    float l = (dot(light_dir, norm) + 1.0) / 2.0;


    float horizon = dot(norm, normalize(norm * vec3(1.0, 0.0, 1.0)));
    float haze = ((dot(light_dir, norm) + horizon) + 1.0) / 2.0;
    vec3 blue = mix(dark_blue, light_blue, pow(horizon, 2.0));
    vec3 sun = sun_color * (10.0 * pow(l, 64.0));

    color = vec4(mix(blue, sun, pow(l, 32.0)), 1.0);
    // color = vec4((v_normal + 1.0) / 2.0, 1.0);
    // color = vec4(1.0, 0.0, 0.0, 1.0);
}
