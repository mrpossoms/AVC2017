#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec3 a_tangent;
layout(location = 3) in vec3 a_texcoord;

out vec2 v_texcoord; // texture coords
out vec3 v_normal;   // normal
out vec3 v_binormal; // binormal (for TBN basis calc)
out vec3 v_pos;      // pixel view space position


uniform mat4 u_view_matrix;
uniform mat4 u_proj_matrix;
uniform mat3 u_normal_matrix;
uniform mat4 u_world_matrix;


void main()
{
	vec3 binormal = cross(a_normal, a_tangent);

	v_texcoord = a_texcoord.xy;
	v_normal   = u_normal_matrix * a_normal;
	v_binormal = u_normal_matrix * a_tangent;
	v_binormal = cross(v_normal, v_binormal);

	vec4 world_space = u_world_matrix * vec4(a_position * 100.0, 1.0);
	vec4 view_space = u_view_matrix * world_space;
	gl_Position = u_proj_matrix * view_space;

	v_pos = normalize(u_normal_matrix * a_position); //view_space.xyz;
}
