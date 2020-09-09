#version 400 core

in vec4 v_screen_space;      // pixel view space position

out vec4 color;

void main()
{
  float depth = v_screen_space.z / v_screen_space.w;
  depth = (depth + 1.0) * 0.5;

  float moment1 = depth;
  float moment2 = depth * depth;
  float dx = dFdx(depth);
  float dy = dFdy(depth);

  moment2 += 0.25 * (dx * dx + dy * dy);

  color = vec4(moment1, moment2, 0.0, 0.0);
}
