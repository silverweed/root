#version 450

#include "instance_data.glsl"

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec2 in_uv;

layout (location = 0) out vec2 frag_uv;
layout (location = 1) out flat vec4 color;

layout (binding = 0) readonly buffer Instance_Data_Buf {
    Instance_Data inst_data[];
};

layout (push_constant) uniform Push_Constant {
  mat4 view_proj;
} pc;

void main() {
    gl_Position = pc.view_proj * inst_data[gl_InstanceIndex].model * vec4(in_pos, 1.0);
    frag_uv = in_uv;
    color = inst_data[gl_InstanceIndex].color_a;
}
