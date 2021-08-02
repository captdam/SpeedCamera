#version 310 es

/* Texture type = R_8 */

precision mediump float;

uniform sampler2D pStage;
uniform vec2 size;
in vec2 textpos;
out vec4 nStage;

void main() {
	ivec2 texelIndex = ivec2(
		int(textpos.x * size.x),
		int(textpos.y * size.y)
	);
	nStage.r = texelFetch(pStage, texelIndex, 0).r;
}
