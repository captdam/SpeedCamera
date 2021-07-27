#version 310 es

precision mediump float;

uniform sampler2D bitmap;

uniform vec2 size;
uniform vec3 maskTop;
uniform vec3 maskMiddle;
uniform vec3 maskBottom;

in vec2 textpos;

out vec4 color;

void main() {
	ivec2 texelIndex = ivec2(
		int(textpos.x * size.x),
		int(textpos.y * size.y)
	);
	vec3 pixelTop = vec3(
		texelFetch(bitmap, texelIndex + ivec2(-1, -1), 0).r,
		texelFetch(bitmap, texelIndex + ivec2( 0, -1), 0).r,
		texelFetch(bitmap, texelIndex + ivec2(+1, -1), 0).r
	);
	vec3 pixelMiddle = vec3(
		texelFetch(bitmap, texelIndex + ivec2(-1,  0), 0).r,
		texelFetch(bitmap, texelIndex + ivec2( 0,  0), 0).r,
		texelFetch(bitmap, texelIndex + ivec2(+1,  0), 0).r
	);
	vec3 pixelBottom = vec3(
		texelFetch(bitmap, texelIndex + ivec2(-1, +1), 0).r,
		texelFetch(bitmap, texelIndex + ivec2( 0, +1), 0).r,
		texelFetch(bitmap, texelIndex + ivec2(+1, +1), 0).r
	);

	float accumTop = dot(pixelTop, maskTop);
	float accumMiddle = dot(pixelMiddle, maskMiddle);
	float accumBottom = dot(pixelBottom, maskBottom);
	float accum = accumTop + accumMiddle + accumBottom;

	color = vec4(accum, accum, accum, 1.0f);
}
