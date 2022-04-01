in highp vec2 pxPos;
out lowp vec4 result; //lowp for RGBA8 video

uniform lowp sampler2D src; //lowp for enum
uniform mediump sampler2D roadmapT1;

/* Defined by client: STEP */
/* Defined by client: DENOISE_BOTTOM */
/* Defined by client: DENOISE_SIDE */

#define RESULT_NOTOBJ 0.0
#define RESULT_OBJECT 0.3
#define RESULT_BOTTOM 0.6
#define RESULT_CBEDGE 1.0

// Return true if found valid pixel in limit
bool searchBottom(lowp sampler2D map, mediump ivec2 idx, mediump int limit) {
	for (mediump ivec2 i = idx; i.y < limit; i.y++) {
		if (texelFetch(map, i, 0).r > 0.0)
			return true;
	}
	return false;
}

bvec2 minPath(lowp sampler2D map, mediump ivec2 start, lowp float threshold, mediump int goal) {
	mediump ivec2 lPtr = start, rPtr = start;

	while (start.x - lPtr.x < goal) {
		mediump ivec2 middle = lPtr + ivec2(-1, 0);
		mediump ivec2 up = lPtr + ivec2(-1, -1);
		mediump ivec2 down = lPtr + ivec2(-1, +1);
		if (texelFetch(map, middle, 0).r > threshold)
			lPtr = middle;
		else if (texelFetch(map, up, 0).r > threshold)
			lPtr = up;
		else if (texelFetch(map, down, 0).r > threshold)
			lPtr = down;
		else
			break;
	}

	while (rPtr.x - start.x < goal) {
		mediump ivec2 middle = rPtr + ivec2(+1, 0);
		mediump ivec2 up = rPtr + ivec2(+1, -1);
		mediump ivec2 down = rPtr + ivec2(+1, +1);
		if (texelFetch(map, middle, 0).r > threshold)
			rPtr = middle;
		else if (texelFetch(map, up, 0).r > threshold)
			rPtr = up;
		else if (texelFetch(map, down, 0).r > threshold)
			rPtr = down;
		else
			break;
	}

	return bvec2(start.x - lPtr.x >= goal, rPtr.x - start.x >= goal);
}

void main() {
	mediump ivec2 srcSize = textureSize(src, 0);
	mediump vec2 srcSizeF = vec2(srcSize);

	ivec2 pxIdx = ivec2(srcSizeF * pxPos);

	lowp float refined;

	do {
		//Not object
		refined = RESULT_NOTOBJ;

		if (texelFetch(src, pxIdx, 0).r == 0.0)
			break;

		//Object, but maybe not edge
		refined = RESULT_OBJECT;

		mediump vec4 pixelRoadPerspX = textureGather(roadmapT1, pxPos, 0);
		mediump float pixelWidth = abs(pixelRoadPerspX[1] - pixelRoadPerspX[0]); //i1_j1 - i0_j1. Use pixel width for both width and height, pixel height is actually close-far axis, object height is proportional to width
		
		mediump int bottomSearchLimit = int(DENOISE_BOTTOM / pixelWidth) + pxIdx.y + 1; //Atleast search 1, at far distance, pixelWidth is very large
		if (searchBottom( src , pxIdx + ivec2(0,1) , min(srcSize.y, bottomSearchLimit) )) //Do not include self
			break;

		//Bottom edge, but maybe not centered bottom edge
		refined = RESULT_BOTTOM;

		//If any side has space (edge not at center pertion)
		mediump int sideSearchDistance = int(DENOISE_SIDE / pixelWidth);
		bvec2 centerEdge = minPath(src, pxIdx, 0.5, int(sideSearchDistance));
		if (!all(centerEdge))
			break;
		
		//Centered bottom edge
		refined = RESULT_CBEDGE;
	} while (false);

	result = vec4(refined);
}
