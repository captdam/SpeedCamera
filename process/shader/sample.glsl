@VS

layout (location = 0) in highp vec2 meshROI;
out mediump vec2 pxPos;

void main() {
	gl_Position = vec4(meshROI.x * 2.0 - 1.0, meshROI.y * 2.0 - 1.0, 0.0, 1.0);
	pxPos = meshROI.xy;
}

@FS

uniform lowp sampler2D src; //Data: Normalied speed [0,255] and y-target offset at 128 [0,128,255]]

in mediump vec2 pxPos;
out lowp vec2 result; //Data: vec2(speed, target_yCoord), resolution is 1/256

#define MIN_SAMPLE_SIZE 2.0

#define SEARCH_TOTAL x
#define SEARCH_COUNT y
mediump vec2 search(lowp float threshold, mediump ivec2 start, lowp int dir) {
	mediump vec2 res = vec2(0.0); //Accumulation, use mediump
	mediump ivec2 idx = start;
	while (true) {
		mediump ivec2 dest;
		lowp float value;

		dest = idx + ivec2(dir, 0); //Middle
		value = texelFetch(src, dest, 0).x;
		if (value > threshold) {
			idx = dest;
			res.SEARCH_TOTAL += value;
			res.SEARCH_COUNT += 1.0;
			continue;
		}

		dest = idx + ivec2(dir, -1); //Up
		value = texelFetch(src, dest, 0).x;
		if (value > threshold) {
			idx = dest;
			res.SEARCH_TOTAL += value;
			res.SEARCH_COUNT += 1.0;
			continue;
		}

		dest = idx + ivec2(dir, +1); //Down
		value = texelFetch(src, dest, 0).x;
		if (value > threshold) {
			idx = dest;
			res.SEARCH_TOTAL += value;
			res.SEARCH_COUNT += 1.0;
			continue;
		}

		break; //Next, next up, and next dwon not found, stop search
	}
	return res;
}

void main() {
	mediump ivec2 pxIdx = ivec2(vec2(textureSize(src, 0)) * pxPos);

	mediump vec2 res = vec2(0.0);

	lowp vec2 current = texelFetch(src, pxIdx, 0).xy; //For pixels has speed mark
	if (current.x > 0.0) {

		mediump vec2 left = search(0.01, pxIdx, -1);
		mediump vec2 right = search(0.01, pxIdx, +1);

		if ( min(left.SEARCH_COUNT, right.SEARCH_COUNT) > MIN_SAMPLE_SIZE && abs(0.5 + left.SEARCH_COUNT - right.SEARCH_COUNT) < 1.0 ) { //Remove small sample (noise); Get center point, left == right or left == right - 1
			mediump float total = left.SEARCH_TOTAL + right.SEARCH_TOTAL;
			mediump float count = left.SEARCH_COUNT + right.SEARCH_COUNT;
			res.x = total / count; //Use avg speed
			res.y = current.y; //Use center pixel's y-value
		}
	}

	result = res;
}
