#version 310 es

precision mediump float;

uniform sampler2D pStage;
uniform vec2 size;
in vec2 textpos;
out vec4 nStage;

uniform sampler2D history;

void main() {
	ivec2 texelIndex = ivec2(
		int(textpos.x * size.x),
		int(textpos.y * size.y)
	);

	nStage.r = texelFetch(pStage, texelIndex, 0).r * 0.2f + texelFetch(history, texelIndex, 0).r * 0.8f;
}
