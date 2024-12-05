#version 450

#include "instance_data.glsl"
#include "quad_inc.glsl"

// XXX: std430? std140?
layout (push_constant) uniform Push_Constant {
    mat4 view_proj;
} pc;

layout (binding = 0) readonly buffer Instance_Data_Buf {
    Instance_Data inst_data[];
};

layout (location = 0) out vec2 frag_uv;
layout (location = 1) out flat vec4 color_a;
layout (location = 2) out flat vec4 color_b;

void main() {
    vec2 pos2d = rect_positions[gl_VertexIndex].xy;
    vec2 uv = rect_uv[gl_VertexIndex];
    gl_Position = pc.view_proj * inst_data[gl_InstanceIndex].model * vec4(pos2d.x, 0.0, pos2d.y, 1.0);
    frag_uv = uv;
    color_a = inst_data[gl_InstanceIndex].color_a;
    color_b = inst_data[gl_InstanceIndex].color_b;
}
