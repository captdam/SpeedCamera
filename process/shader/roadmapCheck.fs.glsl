in vec2 pxPos;
out vec4 result;

uniform sampler2D roadmapT1;
uniform sampler2D roadmapT2;
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
#define MODE_T1 1
#define MODE_T2 2
#define MODE_PERSPGRID 3
#define MODE_OTHORGRID 4

void main() {
	ivec2 pxIdx = ivec2(vec2(textureSize(roadmapT1, 0)) * pxPos);

	vec4 roadPosCurrent =	texelFetch(roadmapT1, pxIdx, 0);
	vec4 roadPosUp =	texelFetchOffset(roadmapT1, pxIdx, 0, ivec2( 0, -1));
	vec4 roadPosDown =	texelFetchOffset(roadmapT1, pxIdx, 0, ivec2( 0, +1));
	vec4 roadPosLeft =	texelFetchOffset(roadmapT1, pxIdx, 0, ivec2(-1,  0));
	vec4 roadPosRight =	texelFetchOffset(roadmapT1, pxIdx, 0, ivec2(+1,  0));
	vec4 roadInfoCurrent =	texelFetch(roadmapT2, pxIdx, 0);

	vec4 color = vec4(0.0);

	if (MODE == MODE_T1) {
		color = roadPosCurrent;
	} else if (MODE == MODE_T2) {
		color = roadInfoCurrent;
	} else if (MODE == MODE_PERSPGRID) {
		if ( sign(roadPosUp.CH_ROADMAP1_PY) != sign(roadPosDown.CH_ROADMAP1_PY) )
			color = vec4(1.0, 0.2, 0.2, 1.0);
		else if ( floor(roadPosLeft.CH_ROADMAP1_PX / cfgF1.x) != floor(roadPosRight.CH_ROADMAP1_PX / cfgF1.x) )
			color = vec4(0.1, 0.1, 0.8, 1.0);
		else if ( floor(roadPosUp.CH_ROADMAP1_PY / cfgF1.y) != floor(roadPosDown.CH_ROADMAP1_PY / cfgF1.y) )
			color = vec4(0.1, 0.8, 0.1, 1.0);
	} else if (MODE == MODE_OTHORGRID) {
		if ( sign(roadPosUp.CH_ROADMAP1_OY) != sign(roadPosDown.CH_ROADMAP1_OY) )
			color = vec4(1.0, 0.2, 0.2, 1.0);
		else if ( floor(roadPosLeft.CH_ROADMAP1_OX / cfgF1.x) != floor(roadPosRight.CH_ROADMAP1_OX / cfgF1.x) )
			color = vec4(0.1, 0.1, 0.8, 1.0);
		else if ( floor(roadPosUp.CH_ROADMAP1_OY / cfgF1.y) != floor(roadPosDown.CH_ROADMAP1_OY / cfgF1.y) )
			color = vec4(0.1, 0.8, 0.1, 1.0);
	}

	result = /*vec4(*/color/*, 1.0)*/;
}
