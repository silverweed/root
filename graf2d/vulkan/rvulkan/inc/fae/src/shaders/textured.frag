#version 450

layout (location = 0) in vec2 frag_uv;
layout (location = 1) in vec4 color;

layout (location = 0) out vec4 out_color;

layout (binding = 1) uniform sampler2D tex_sampler;

void main() {
    out_color = color * texture(tex_sampler, frag_uv);
}
