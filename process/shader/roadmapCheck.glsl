@VS

layout (location = 0) in highp vec2 meshFocusRegion;
out highp vec2 pxPos;

void main() {
	gl_Position = vec4(meshFocusRegion.x * 2.0 - 1.0, meshFocusRegion.y * 2.0 - 1.0, 0.0, 1.0);
	pxPos = meshFocusRegion.xy;
}

@FS

uniform highp sampler2DArray roadmap;

in highp vec2 pxPos;
out highp vec4 result;

#define CH_ROADMAP1_PX x
#define CH_ROADMAP1_PY y
#define CH_ROADMAP1_OX z
#define CH_ROADMAP1_OY w
#define CH_ROADMAP2_SEARCH_UP x
#define CH_ROADMAP2_SEARCH_DOWN y
#define CH_ROADMAP2_LOOKUP_P2O z
#define CH_ROADMAP2_LOOKUP_O2P w

#define GRID_X 3.3
#define GRID_Y 10.0

void main() {
	highp ivec2 pxIdx = ivec2(vec2(textureSize(roadmap, 0)) * pxPos);
	highp ivec3 pxIdxT1 = ivec3(pxIdx, 0.0);
	highp ivec3 pxIdxT2 = ivec3(pxIdx, 1.0);

	highp vec4 roadPosCurrent =	texelFetch(roadmap, pxIdxT1, 0);
	highp vec4 roadPosUp =		texelFetchOffset(roadmap, pxIdxT1, 0, ivec2( 0, -1));
	highp vec4 roadPosDown =	texelFetchOffset(roadmap, pxIdxT1, 0, ivec2( 0, +1));
	highp vec4 roadPosLeft =	texelFetchOffset(roadmap, pxIdxT1, 0, ivec2(-1,  0));
	highp vec4 roadPosRight =	texelFetchOffset(roadmap, pxIdxT1, 0, ivec2(+1,  0));
	highp vec4 roadInfoCurrent =	texelFetch(roadmap, pxIdxT2, 0);

	highp vec4 color = vec4(0.0);

	//color = roadPosCurrent.xyzw;

	//color = roadInfoCurrent.xyzw;

	if ( sign(roadPosUp.CH_ROADMAP1_PY) != sign(roadPosDown.CH_ROADMAP1_PY) )
		color = vec4(1.0, 0.2, 0.2, 1.0);
	else if ( floor(roadPosLeft.CH_ROADMAP1_PX / GRID_X) != floor(roadPosRight.CH_ROADMAP1_PX / GRID_X) )
		color = vec4(0.1, 0.1, 0.8, 1.0);
	else if ( floor(roadPosUp.CH_ROADMAP1_PY / GRID_Y) != floor(roadPosDown.CH_ROADMAP1_PY / GRID_Y) )
		color = vec4(0.1, 0.8, 0.1, 1.0);
		
	/*if ( sign(roadPosUp.CH_ROADMAP1_OY) != sign(roadPosDown.CH_ROADMAP1_OY) )
		color = vec4(1.0, 0.2, 0.2, 1.0);
	else if ( floor(roadPosLeft.CH_ROADMAP1_OX / GRID_X) != floor(roadPosRight.CH_ROADMAP1_OX / GRID_X) )
		color = vec4(0.1, 0.1, 0.8, 1.0);
	else if ( floor(roadPosUp.CH_ROADMAP1_OY / GRID_Y) != floor(roadPosDown.CH_ROADMAP1_OY / GRID_Y) )
		color = vec4(0.1, 0.8, 0.1, 1.0);*/

	result = color;
}
