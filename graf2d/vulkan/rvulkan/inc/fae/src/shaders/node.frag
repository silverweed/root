#version 450

layout (location = 0) in vec2 frag_uv;
layout (location = 1) in vec4 color_a;
layout (location = 2) in vec4 color_b;

layout (location = 0) out vec4 out_color;

void main()
{
    out_color = mix(color_a, color_b, frag_uv.y);
}
