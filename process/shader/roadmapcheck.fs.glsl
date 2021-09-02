#version 310 es

precision mediump float;

uniform sampler2D roadmap;
uniform vec2 size;
in vec2 textpos;
out vec4 nStage;

void main() {
	ivec2 texelIndex = ivec2(
		int(textpos.x * size.x),
		int(textpos.y * size.y)
	);
	vec4 up =	texelFetchOffset(roadmap, texelIndex, 0, ivec2( 0, -1));
	vec4 down =	texelFetchOffset(roadmap, texelIndex, 0, ivec2( 0, +1));
	vec4 left =	texelFetchOffset(roadmap, texelIndex, 0, ivec2(-1,  0));
	vec4 right =	texelFetchOffset(roadmap, texelIndex, 0, ivec2(+1,  0));
	
	float diff = 0.0;
	if (floor(left) != floor(right))
		diff = 1.0;
	if (floor(up) != floor(down))
		diff = 1.0;
	nStage = vec4(diff, diff, diff, 1.0);
}
