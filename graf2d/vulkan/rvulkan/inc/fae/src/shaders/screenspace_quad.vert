#version 450

#include "instance_data.glsl"
#include "quad_inc.glsl"

layout (binding = 0) readonly buffer Instance_Data_Buf {
    Instance_Data inst_data[];
};

layout (push_constant) uniform Push_Constants {
  // in pixels!
  vec2 inv_viewport_size;
};

layout (location = 0) out vec2 frag_uv;
layout (location = 1) out flat vec4 color;

void main() {
    vec2 pos2d = rect_positions[gl_VertexIndex].xy;
    vec2 uv = rect_uv[gl_VertexIndex];
    // NOTE: `model` contains the size in pixels of the quad. By multiplying by inv_viewport_size,
    // we make sure that it will have that exact size regardless of the viewport size.
    gl_Position = inst_data[gl_InstanceIndex].model * vec4(pos2d.xy, 0.0, 1.0);
    gl_Position.xy *= inv_viewport_size;
    frag_uv = uv;
    color = inst_data[gl_InstanceIndex].color_a;
}
