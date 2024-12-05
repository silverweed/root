#version 450

layout (location = 0) in vec3 dir;

layout (location = 0) out vec4 out_color;

layout (push_constant) uniform Push_Constant {
	layout (offset = 64) vec4 color_a;
	vec4 color_b;
	vec4 color_c;
	float time;
} pc;

void add_celestial_body(vec3 body_dir, vec3 color, float cos_ang_size, float cos_halo_ang_size, inout vec3 col)
{
    float cos_ang = dot(normalize(dir), normalize(body_dir));
    col = mix(col, color, smoothstep(cos_ang_size - 0.0001, cos_ang_size, cos_ang));
    col = mix(col, color, 0.35 * smoothstep(cos_halo_ang_size, cos_ang_size, cos_ang));
}

// https://stackblitz.com/edit/starry-skydome?file=StarrySkyShader.jsb
vec4 permute(vec4 x){return mod(((x*34.0)+1.0)*x, 289.0);}
vec4 taylorInvSqrt(vec4 r){return 1.79284291400159 - 0.85373472095314 * r;}
vec3 fade(vec3 t) {return t*t*t*(t*(t*6.0-15.0)+10.0);}
float cnoise(vec3 P)
{
	vec3 Pi0 = floor(P); // Integer part for indexing
	vec3 Pi1 = Pi0 + vec3(1.0); // Integer part + 1
	Pi0 = mod(Pi0, 289.0);
	Pi1 = mod(Pi1, 289.0);
	vec3 Pf0 = fract(P); // Fractional part for interpolation
	vec3 Pf1 = Pf0 - vec3(1.0); // Fractional part - 1.0
	vec4 ix = vec4(Pi0.x, Pi1.x, Pi0.x, Pi1.x);
	vec4 iy = vec4(Pi0.yy, Pi1.yy);
	vec4 iz0 = Pi0.zzzz;
	vec4 iz1 = Pi1.zzzz;

	vec4 ixy = permute(permute(ix) + iy);
	vec4 ixy0 = permute(ixy + iz0);
	vec4 ixy1 = permute(ixy + iz1);

	vec4 gx0 = ixy0 / 7.0;
	vec4 gy0 = fract(floor(gx0) / 7.0) - 0.5;
	gx0 = fract(gx0);
	vec4 gz0 = vec4(0.5) - abs(gx0) - abs(gy0);
	vec4 sz0 = step(gz0, vec4(0.0));
	gx0 -= sz0 * (step(0.0, gx0) - 0.5);
	gy0 -= sz0 * (step(0.0, gy0) - 0.5);

	vec4 gx1 = ixy1 / 7.0;
	vec4 gy1 = fract(floor(gx1) / 7.0) - 0.5;
	gx1 = fract(gx1);
	vec4 gz1 = vec4(0.5) - abs(gx1) - abs(gy1);
	vec4 sz1 = step(gz1, vec4(0.0));
	gx1 -= sz1 * (step(0.0, gx1) - 0.5);
	gy1 -= sz1 * (step(0.0, gy1) - 0.5);

	vec3 g000 = vec3(gx0.x,gy0.x,gz0.x);
	vec3 g100 = vec3(gx0.y,gy0.y,gz0.y);
	vec3 g010 = vec3(gx0.z,gy0.z,gz0.z);
	vec3 g110 = vec3(gx0.w,gy0.w,gz0.w);
	vec3 g001 = vec3(gx1.x,gy1.x,gz1.x);
	vec3 g101 = vec3(gx1.y,gy1.y,gz1.y);
	vec3 g011 = vec3(gx1.z,gy1.z,gz1.z);
	vec3 g111 = vec3(gx1.w,gy1.w,gz1.w);

	vec4 norm0 = taylorInvSqrt(vec4(dot(g000, g000), dot(g010, g010), dot(g100, g100), dot(g110, g110)));
	g000 *= norm0.x;
	g010 *= norm0.y;
	g100 *= norm0.z;
	g110 *= norm0.w;
	vec4 norm1 = taylorInvSqrt(vec4(dot(g001, g001), dot(g011, g011), dot(g101, g101), dot(g111, g111)));
	g001 *= norm1.x;
	g011 *= norm1.y;
	g101 *= norm1.z;
	g111 *= norm1.w;

	float n000 = dot(g000, Pf0);
	float n100 = dot(g100, vec3(Pf1.x, Pf0.yz));
	float n010 = dot(g010, vec3(Pf0.x, Pf1.y, Pf0.z));
	float n110 = dot(g110, vec3(Pf1.xy, Pf0.z));
	float n001 = dot(g001, vec3(Pf0.xy, Pf1.z));
	float n101 = dot(g101, vec3(Pf1.x, Pf0.y, Pf1.z));
	float n011 = dot(g011, vec3(Pf0.x, Pf1.yz));
	float n111 = dot(g111, Pf1);

	vec3 fade_xyz = fade(Pf0);
	vec4 n_z = mix(vec4(n000, n100, n010, n110), vec4(n001, n101, n011, n111), fade_xyz.z);
	vec2 n_yz = mix(n_z.xy, n_z.zw, fade_xyz.y);
	float n_xyz = mix(n_yz.x, n_yz.y, fade_xyz.x);
	return 2.2 * n_xyz;
}

void add_stars(vec3 dir, inout vec3 col)
{
    float freq = 1.1;
    float noise = cnoise(dir * freq);

    vec3 env_c1 = vec3(0.051, 0.102, 0.184);
    vec3 env_c2 = vec3(0.059, 0.525, 0.51);
	vec3 backgroundColor = mix(env_c1, env_c2, noise);
	col = mix(col, backgroundColor.rgb, 0.3);

    float clusterSize = 0.2;
    float starSize = 0.005;
	float scaledClusterSize = (1.0/clusterSize);
	float scaledStarSize = (1.0/starSize);
	vec3 noiseOffset = vec3(100.01, 100.01, 100.01);
	float clusterStrength = 0.15;
	float starDensity = 0.10;

	float cs = pow(cnoise(scaledClusterSize*dir+noiseOffset),1.0/clusterStrength) 
		+ cnoise(scaledStarSize*dir);

	float cc =clamp(pow(cs, 1.0/starDensity),0.0,1.0);
	vec3 starColor = 0.5*vec3(cc);

	col = min(col + starColor, vec3(1.0));
}

void main() {
	float ldir = length(dir);
    float sin_asc = dir.y / ldir; // [-1, 1]

    // gradient
    float y = 0.5 * (sin_asc + 1);
    float step = 0.5;
    vec3 c = mix(pc.color_a.rgb, pc.color_b.rgb, smoothstep(0.0, step, y));
    c = mix(c, pc.color_c.rgb, smoothstep(step, 1.0, y));

    // stars
    add_stars(dir, c);

    // moon
    const vec3 white = vec3(1.0, 1.0, 1.0);
	add_celestial_body(vec3(0.5, 0.5, 0.5), white, 0.999, 0.96, c);

    // horizon
    float decl = atan(dir.z, dir.x); // [0, 2pi]
	vec3 hdir = vec3(dir.x, 0.0, dir.z);
    float off = 0.03 * cnoise(1.5 * hdir) + 0.015 * cnoise(5.78 * hdir);
    float horiz_mask = smoothstep(-0.002 + off, 0.002 + off, dir.y);
    c = mix(c, vec3(0.05, 0.05, 0.08), 1.0 - horiz_mask);

    out_color = vec4(c, 1.0);
    out_color = pow(out_color, vec4(2.2));
}
