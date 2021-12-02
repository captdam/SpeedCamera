#version 310 es

precision highp float;

in vec2 currentPos;
out vec4 result;

uniform sampler2D roadmapT1;
uniform highp isampler2D roadmapT2;
uniform ivec4 cfgI1;
uniform vec4 cfgF1;

#define CH_ROADMAP1_PX x
#define CH_ROADMAP1_PY y
#define CH_ROADMAP1_OX z
#define CH_ROADMAP1_OY w
#define CH_ROADMAP2_SEARCH_X x
#define CH_ROADMAP2_SEARCH_Y y
#define CH_ROADMAP2_LOOKUP_P2O z
#define CH_ROADMAP2_LOOKUP_O2P w

#define MODE cfgI1.x
#define MODE_PRINTT1XY 1
#define MODE_PRINTT1ZW 2
#define MODE_PRINTT2XY 3
#define MODE_PRINTT2ZW 4
#define MODE_PERSPGRID 5
#define MODE_OTHORGRID 6

void main() {
	ivec2 currentIdx = ivec2( vec2(textureSize(roadmapT1, 0)) * currentPos );

	vec4 roadPosCurrent =	texelFetch(roadmapT1, currentIdx, 0);
	vec4 roadPosUp =	texelFetchOffset(roadmapT1, currentIdx, 0, ivec2( 0, -1));
	vec4 roadPosDown =	texelFetchOffset(roadmapT1, currentIdx, 0, ivec2( 0, +1));
	vec4 roadPosLeft =	texelFetchOffset(roadmapT1, currentIdx, 0, ivec2(-1,  0));
	vec4 roadPosRight =	texelFetchOffset(roadmapT1, currentIdx, 0, ivec2(+1,  0));
	ivec4 roadInfoCurrent =	texelFetch(roadmapT2, currentIdx, 0);

	vec3 color = vec3(0.0, 0.0, 0.0);

	if (MODE == MODE_PRINTT1XY) {
		color = vec3(roadPosCurrent.xy, 0.0);
	} else if (MODE == MODE_PRINTT1ZW) {
		color = vec3(roadPosCurrent.zw, 0.0);
	} else if (MODE == MODE_PRINTT2XY) {
		color = vec3(roadInfoCurrent.xy, 0.0);
	} else if (MODE == MODE_PRINTT2ZW) {
		color = vec3(roadInfoCurrent.zw, 0.0);
	} else if (MODE == MODE_PERSPGRID) {
		if ( sign(roadPosUp.CH_ROADMAP1_PY) != sign(roadPosDown.CH_ROADMAP1_PY) )
			color = vec3(1.0, 0.2, 0.2);
		else if ( floor(roadPosLeft.CH_ROADMAP1_PX / cfgF1.x) != floor(roadPosRight.CH_ROADMAP1_PX / cfgF1.x) )
			color = vec3(0.1, 0.6, 0.1);
		else if ( floor(roadPosUp.CH_ROADMAP1_PY / cfgF1.y) != floor(roadPosDown.CH_ROADMAP1_PY / cfgF1.y) )
			color = vec3(0.1, 0.1, 0.6);
	} else if (MODE == MODE_OTHORGRID) {
		if ( sign(roadPosUp.CH_ROADMAP1_OY) != sign(roadPosDown.CH_ROADMAP1_OY) )
			color = vec3(1.0, 0.2, 0.2);
		else if ( floor(roadPosLeft.CH_ROADMAP1_OX / cfgF1.x) != floor(roadPosRight.CH_ROADMAP1_OX / cfgF1.x) )
			color = vec3(0.1, 0.6, 0.1);
		else if ( floor(roadPosUp.CH_ROADMAP1_OY / cfgF1.y) != floor(roadPosDown.CH_ROADMAP1_OY / cfgF1.y) )
			color = vec3(0.1, 0.1, 0.6);
	} else {
		color = cfgF1.xyz;
	}

	result = vec4(color, 1.0);
}
