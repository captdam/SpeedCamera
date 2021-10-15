#version 310 es

precision highp float;

uniform sampler2D roadmap; //{road-x, road-y, search-x, search-y}
uniform vec2 size;
in vec2 textpos;
out vec4 nStage;

#define POS_X x
#define POS_Y y
#define SEARCH_X z
#define SEARCH_Y w

void main() {
	ivec2 texelIndex = ivec2(
		int(textpos.x * size.x),
		int(textpos.y * size.y)
	);

	//Position
	vec4 up =	texelFetchOffset(roadmap, texelIndex, 0, ivec2( 0, -1));
	vec4 down =	texelFetchOffset(roadmap, texelIndex, 0, ivec2( 0, +1));
	vec4 left =	texelFetchOffset(roadmap, texelIndex, 0, ivec2(-1,  0));
	vec4 right =	texelFetchOffset(roadmap, texelIndex, 0, ivec2(+1,  0));
	
	const float p = 5.0;
	vec3 diff = vec3(0.0, 0.0, 0.0);
	if ( (up.POS_Y < 0.0 && down.POS_Y > 0.0) || (up.POS_Y > 0.0 && down.POS_Y < 0.0) )
		diff = vec3(1.0, 0.0, 0.0);
	else if (floor(left.POS_X / p) != floor(right.POS_X / p))
		diff = vec3(0.8, 0.8, 0.8);
	else if (floor(up.POS_Y / p) != floor(down.POS_Y / p))
		diff = vec3(0.8, 0.8, 0.8);
	nStage = vec4(diff, 1.0);

/*	//Search region
	float dx = texelFetch(roadmap, texelIndex, 0).SEARCH_X;
	float dy = texelFetch(roadmap, texelIndex, 0).SEARCH_Y;
	nStage = vec4(dx/200.0, 1.0, 1.0, 1.0);*/
}
