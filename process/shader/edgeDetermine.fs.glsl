in vec2 currentPos;
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

/** Return the end position of continue path in left and right direction 
 * Always choose hozizontal (x,0) if that point is valid; otherwise, choose up(x,-1) or down(x,-1) 
 * return {leftEnd.xy, rightEnd.xy}
 */
ivec4 continuePath(sampler2D map, ivec2 start) {
	ivec4 box = ivec4(0.0, 0.0, textureSize(map, 0));
	ivec2 lPtr = start, rPtr = start;

	while (true) {
		ivec2 middle = lPtr + ivec2(-1, 0);
		ivec2 up = lPtr + ivec2(-1, -1);
		ivec2 down = lPtr + ivec2(-1, +1);
		if ( texelFetch(map, middle, 0).r > 0.0 && inBox(middle, box) )
			lPtr = middle;
		else if ( texelFetch(map, up, 0).r > 0.0 && inBox(up, box) )
			lPtr = up;
		else if ( texelFetch(map, down, 0).r > 0.0 && inBox(down, box) )
			lPtr = down;
		else
			break;
	}

	while (true) {
		ivec2 middle = rPtr + ivec2(+1, 0);
		ivec2 up = rPtr + ivec2(+1, -1);
		ivec2 down = rPtr + ivec2(+1, +1);
		if ( texelFetch(map, middle, 0).r > 0.0 && inBox(middle, box) )
			rPtr = middle;
		else if ( texelFetch(map, up, 0).r > 0.0 && inBox(up, box) )
			rPtr = up;
		else if ( texelFetch(map, down, 0).r > 0.0 && inBox(down, box) )
			rPtr = down;
		else
			break;
	}

	return ivec4(lPtr, rPtr);
}

void main() {
	ivec2 currentIdx = ivec2( vec2(textureSize(current, 0)) * currentPos );

	/* Current frame */
	ivec4 endPosCurrent = continuePath(current, currentIdx);
	float detCurrent = endPosCurrent.z- endPosCurrent.x> CURRENT_THRESHOLD ? 1.0 : 0.0;

	/* Previous frame */
	ivec4 endPosPrevious = continuePath(previous, currentIdx);
	float detPrevious = endPosPrevious.z - endPosPrevious.x > PREVIOUS_THRESHOLD ? 1.0 : 0.0;

	/** Edge of current frame and previous frame 
	 * Current in R, previous in G
	 */
	result = vec4(detCurrent, detPrevious, 0.0, step(min(detCurrent, detPrevious), 0.0));
}
