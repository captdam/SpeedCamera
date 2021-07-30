#version 310 es

/* Texture type = R_8 */

precision mediump float;

uniform sampler2D pStage;

uniform vec2 size;

uniform FilterMask {
	vec3 maskTop;
	vec3 maskMiddle;
	vec3 maskBottom;
};

in vec2 textpos;

out vec4 nStage;

void main() {
	ivec2 texelIndex = ivec2(
		int(textpos.x * size.x),
		int(textpos.y * size.y)
	);
	vec3 pixelTop = vec3(
		texelFetch(pStage, texelIndex + ivec2(-1, -1), 0).r,
		texelFetch(pStage, texelIndex + ivec2( 0, -1), 0).r,
		texelFetch(pStage, texelIndex + ivec2(+1, -1), 0).r
	);
	vec3 pixelMiddle = vec3(
		texelFetch(pStage, texelIndex + ivec2(-1,  0), 0).r,
		texelFetch(pStage, texelIndex + ivec2( 0,  0), 0).r,
		texelFetch(pStage, texelIndex + ivec2(+1,  0), 0).r
	);
	vec3 pixelBottom = vec3(
		texelFetch(pStage, texelIndex + ivec2(-1, +1), 0).r,
		texelFetch(pStage, texelIndex + ivec2( 0, +1), 0).r,
		texelFetch(pStage, texelIndex + ivec2(+1, +1), 0).r
	);

	float accumTop = dot(pixelTop, maskTop);
	float accumMiddle = dot(pixelMiddle, maskMiddle);
	float accumBottom = dot(pixelBottom, maskBottom);
	float accum = accumTop + accumMiddle + accumBottom;

	nStage.r = accum;
}
