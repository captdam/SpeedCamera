in vec2 pxPos;
out vec4 result;

uniform sampler2D src;

/* Defined by client: DENOISE_BOTTOM */
/* Defined by client: DENOISE_SIDE */

bvec2 minPath(sampler2D map, ivec2 start, float threshold, int goal) {
	ivec2 lPtr = start, rPtr = start;

	while (start.x - lPtr.x < goal) {
		ivec2 middle = lPtr + ivec2(-1, 0);
		ivec2 up = lPtr + ivec2(-1, -1);
		ivec2 down = lPtr + ivec2(-1, +1);
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
		ivec2 middle = rPtr + ivec2(+1, 0);
		ivec2 up = rPtr + ivec2(+1, -1);
		ivec2 down = rPtr + ivec2(+1, +1);
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
	ivec2 pxIdx = ivec2(vec2(textureSize(src, 0)) * pxPos);

	float refined = 0.0;
	bool ok = true;

	if (ok) {
		if (texelFetch(src, pxIdx, 0).r == 0.0)
			ok = false;
		refined = 0.0; //Not valid object
	}

	if (ok) {
		for (int i = 1; i < DENOISE_BOTTOM; i++) {
			ivec2 idx = pxIdx + ivec2(0, i);
			if (texelFetch(src, idx, 0).r > 0.0) {
				ok = false;
				break;
			}
		}
		refined = 0.3; //Object but fail edge detection
	}

/*	if (ok) {
		for (int i = 1; i < DENOISE_SIDE; i++) {
			ivec2 idxl = pxIdx + ivec2(-i, 0);
			ivec2 idxr = pxIdx + ivec2(+i, 0);
			if (
				texelFetch(src, idxl, 0).r == 0.0 ||
				texelFetch(src, idxr, 0).r == 0.0
			) {
				ok = false;
				break;
			}
		}
		refined = 0.6; //Edge but fail center check
	}*/

	if (ok) {
		bvec2 centerEdge = minPath(src, pxIdx, 0.5, DENOISE_SIDE);
		ok = all(centerEdge);
		refined = 0.6; //Edge but fail center check
	}
	
	if (ok) {
		refined = 1.0; //Center edge
	}

	result = vec4(refined);
}
