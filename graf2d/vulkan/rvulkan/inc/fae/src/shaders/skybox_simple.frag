#version 450

layout (location = 0) in vec3 dir;

layout (location = 0) out vec4 out_color;

layout (push_constant) uniform Push_Constant {
	layout (offset = 64) vec4 color_a;
	vec4 color_b;
	vec4 color_c;
	float time;
} pc;

void main() {
	float ldir = length(dir);
    float sin_asc = dir.y / ldir; // [-1, 1]

    // gradient
    float y = 0.5 * (sin_asc + 1);
    float step = 0.5;
    vec3 c = mix(pc.color_a.rgb, pc.color_b.rgb, smoothstep(0.0, step, y));
    c = mix(c, pc.color_c.rgb, smoothstep(step, 1.0, y));

    out_color = vec4(c, 1.0);
    out_color = pow(out_color, vec4(2.2));
}
