#version 450

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec2 in_uv; // unused

layout (location = 0) out vec3 dir;

layout (push_constant) uniform Push_Constant {
	mat4 view_proj;
} pc;


void main()
{
	dir = in_pos;
	vec4 pos = vec4(in_pos, 0);
	gl_Position = pc.view_proj * pos;
	gl_Position.z = 0; // infinite depth
}
