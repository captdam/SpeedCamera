#version 310 es

precision mediump float;

uniform sampler2D ta;
uniform sampler2D tb;
uniform vec2 size;
in vec2 textpos;
out vec4 nStage;

uniform sampler2D history;
uniform sampler2D roadmap;

void main() {
	ivec2 texelIndex = ivec2(
		int(textpos.x * size.x),
		int(textpos.y * size.y)
	);

	vec4 a = texelFetch(ta, texelIndex, 0);
	vec4 b = texelFetch(tb, texelIndex, 0);
	
	nStage = a - b;
}
