#version 450

layout (location = 0) in vec2 frag_uv;
layout (location = 1) flat in vec4 color;

layout (location = 0) out vec4 out_color;

layout (binding = 1) uniform sampler2D font_atlas;

float median(float r, float g, float b)
{
    return max(min(r, g), min(max(r, g), b));
}

void main()
{
    vec3 samp = texture(font_atlas, frag_uv).rgb;
    float sig_dist = median(samp.r, samp.g, samp.b);
    float w = fwidth(sig_dist);
    float opacity = smoothstep(0.5 - w, 0.5 + w, sig_dist);
        
    out_color = color * vec4(1.0, 1.0, 1.0, opacity);
}
