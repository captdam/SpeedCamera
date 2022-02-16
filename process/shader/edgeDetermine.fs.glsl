in vec2 pxPos;
out vec4 result;

uniform sampler2D current;
uniform sampler2D previous;

/* Defined by client: ivec4 threshold */
#define CURRENT_THRESHOLD threshold.x
#define PREVIOUS_THRESHOLD threshold.y

/** Is p inside the box 
 * leftTop = box.xy and rightBottom = box.zw
 */
bool inBox(ivec2 p, ivec4 box) {
	return p.x >= box.x && p.y >= box.y && p.x < box.z && p.y < box.w;
}

bvec2 minPath(sampler2D map, ivec2 start, int goal) {
	ivec2 lPtr = start, rPtr = start;

	while (start.x - lPtr.x < goal) {
		ivec2 middle = lPtr + ivec2(-1, 0);
		ivec2 up = lPtr + ivec2(-1, -1);
		ivec2 down = lPtr + ivec2(-1, +1);
		if (texelFetch(map, middle, 0).r > 0.0)
			lPtr = middle;
		else if (texelFetch(map, up, 0).r > 0.0)
			lPtr = up;
		else if (texelFetch(map, down, 0).r > 0.0)
			lPtr = down;
		else
			break;
	}

	while (rPtr.x - start.x < goal) {
		ivec2 middle = rPtr + ivec2(+1, 0);
		ivec2 up = rPtr + ivec2(+1, -1);
		ivec2 down = rPtr + ivec2(+1, +1);
		if (texelFetch(map, middle, 0).r > 0.0)
			rPtr = middle;
		else if (texelFetch(map, up, 0).r > 0.0)
			rPtr = up;
		else if (texelFetch(map, down, 0).r > 0.0)
			rPtr = down;
		else
			break;
	}

	return bvec2(start.x - lPtr.x >= goal, rPtr.x - start.x >= goal);
}

void main() {
	ivec2 pxIdx = ivec2(vec2(textureSize(current, 0)) * pxPos);

	bvec2 currentTest = minPath(current, pxIdx, CURRENT_THRESHOLD);
	float currentDet = all(currentTest) ? 1.0 : 0.0;
	bvec2 previousTest = minPath(previous, pxIdx, PREVIOUS_THRESHOLD);
	float previousDet = any(previousTest) ? 1.0 : 0.0;

	/** Edge of current frame and previous frame 
	 * Current in R, previous in G
	 * Hide this result (when display intermidiate result) if there neither edge detected
	 */
	result = vec4(currentDet, previousDet, 0.0, step(0.0, max(currentDet, previousDet)));
}
