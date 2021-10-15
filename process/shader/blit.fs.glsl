#version 310 es

precision highp float;

uniform sampler2D pStage;
uniform vec2 size;
in vec2 textpos;
out vec4 nStage;

void main() {
	ivec2 texelIndex = ivec2(
		int(textpos.x * size.x),
		int(textpos.y * size.y)
	);
	nStage = texelFetch(pStage, texelIndex, 0);
}
