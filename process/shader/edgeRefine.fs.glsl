in highp vec2 pxPos;
out lowp vec4 result; //lowp for RGBA8 video

uniform lowp sampler2D src; //lowp for enum

/* Defined by client: DENOISE_BOTTOM */
/* Defined by client: DENOISE_SIDE */

#define RESULT_NOTOBJ 0.0
#define RESULT_OBJECT 0.3
#define RESULT_BOTTOM 0.7
#define RESULT_CBEDGE 1.0

// Return true if found valid pixel in limit
bool searchBottom(lowp sampler2D map, mediump vec2 pos, mediump float step, mediump float limit) {
	for (mediump vec2 p = pos; pos.y < limit; pos.y += step) {
		if (dot(textureGatherOffset(map, p, ivec2(0,2), 0), vec4(1.0)) > 0.0)
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

		//If bottom clerance not enough (object or inner bottom edge)
		if (searchBottom(src, pxPos, 2.0 / srcSizeF.y, min(1.0, pxPos.y + DENOISE_BOTTOM))) //2.0: textureGather() takes 2*2 titles
			break;

		//Bottom edge, but maybe not centered bottom edge
		refined = RESULT_BOTTOM;

		bvec2 centerEdge = minPath(src, pxIdx, 0.5, int(DENOISE_SIDE * srcSizeF.x));
		if (!all(centerEdge))
			break;
		
		//Centered bottom edge
		refined = RESULT_CBEDGE;
	} while (false);

	result = vec4(refined);
}
