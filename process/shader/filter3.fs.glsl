#version 310 es

precision mediump float;

uniform sampler2D pStage;
uniform vec2 size;
in vec2 textpos;
out vec4 nStage;

layout (std140) uniform FilterMask {
	vec3 maskTop;
	vec3 maskMiddle;
	vec3 maskBottom;
};

void main() {
	ivec2 texelIndex = ivec2(
		int(textpos.x * size.x),
		int(textpos.y * size.y)
	);

	vec4 accum = vec4(0.0, 0.0, 0.0, 0.0);
	accum += texelFetchOffset(pStage, texelIndex, 0, ivec2(-1, -1)) * maskTop.x;
	accum += texelFetchOffset(pStage, texelIndex, 0, ivec2( 0, -1)) * maskTop.y;
	accum += texelFetchOffset(pStage, texelIndex, 0, ivec2(+1, -1)) * maskTop.z;
	accum += texelFetchOffset(pStage, texelIndex, 0, ivec2(-1,  0)) * maskMiddle.x;
	accum += texelFetchOffset(pStage, texelIndex, 0, ivec2( 0,  0)) * maskMiddle.y;
	accum += texelFetchOffset(pStage, texelIndex, 0, ivec2(+1,  0)) * maskMiddle.z;
	accum += texelFetchOffset(pStage, texelIndex, 0, ivec2(-1, +1)) * maskBottom.x;
	accum += texelFetchOffset(pStage, texelIndex, 0, ivec2( 0, +1)) * maskBottom.y;
	accum += texelFetchOffset(pStage, texelIndex, 0, ivec2(+1, +1)) * maskBottom.z;

	nStage = vec4(accum.rgb, 1.0);
}
