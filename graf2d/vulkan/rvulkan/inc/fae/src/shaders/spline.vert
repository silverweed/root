#version 450

#include "instance_data.glsl"

layout (location = 0) in vec4 in_pos;

layout (location = 0) out flat vec4 color;

layout (binding = 0) readonly buffer Instance_Data_Buf {
    Instance_Data inst_data[];
};

layout (push_constant) uniform Push_Constant {
  mat4 view_proj;
} pc;

void main() 
{
    vec4 wpos = inst_data[gl_InstanceIndex].model * vec4(in_pos.xyz, 1.0);
    gl_Position = pc.view_proj * wpos;
    color = inst_data[gl_InstanceIndex].color_a;
}
