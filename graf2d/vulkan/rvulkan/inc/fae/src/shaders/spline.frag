#version 450

layout (location = 0) in flat vec4 color;

layout (location = 0) out vec4 out_color;

void main() 
{
    out_color = color;
}
