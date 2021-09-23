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

/*	//Position
	vec4 up =	texelFetchOffset(roadmap, texelIndex, 0, ivec2( 0, -1));
	vec4 down =	texelFetchOffset(roadmap, texelIndex, 0, ivec2( 0, +1));
	vec4 left =	texelFetchOffset(roadmap, texelIndex, 0, ivec2(-1,  0));
	vec4 right =	texelFetchOffset(roadmap, texelIndex, 0, ivec2(+1,  0));
	
	vec3 diff = vec3(0.0, 0.0, 0.0);
	if ( (up.y < 0.0 && down.y > 0.0) || (up.y > 0.0 && down.y < 0.0) )
		diff = vec3(1.0, 0.0, 0.0);
	else if (floor(left) != floor(right))
		diff = vec3(0.8, 0.8, 0.8);
	else if (floor(up) != floor(down))
		diff = vec3(0.8, 0.8, 0.8);
	nStage = vec4(diff, 1.0);*/

	//Search region
	float dx = texelFetch(roadmap, texelIndex, 0).z;
	float dy = texelFetch(roadmap, texelIndex, 0).w;
	nStage = vec4(dx/200.0, 1.0, 1.0, 1.0);
}
