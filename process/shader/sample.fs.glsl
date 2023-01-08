in highp vec2 pxPos;
out lowp vec2 result; //Data: vec2(speed, target_yCoord), resolution is 1/256
#define CH_ROADDISTANCE x
#define CH_TARGETYCOORD y

uniform mediump sampler2D src; //Data: vec2(speed, target_yCoord)

/* Defined by client: MIN_SAMPLE_SIZE float */

#define SEARCH_TOTAL x
#define SEARCH_COUNT y
highp vec2 search(highp sampler2D map, highp float threshold, mediump ivec2 start, lowp int dir) {
	highp vec2 res = vec2(0.0); //Accumulation, use highp
	mediump ivec2 idx = start;
	while (true) {
		mediump ivec2 dest;
		highp float value;

		dest = idx + ivec2(dir, 0); //Middle
		value = texelFetch(map, dest, 0).CH_ROADDISTANCE;
		if (value > threshold) {
			idx = dest;
			res.SEARCH_TOTAL += value;
			res.SEARCH_COUNT += 1.0;
			continue;
		}

		dest = idx + ivec2(dir, -1); //Up
		value = texelFetch(map, dest, 0).CH_ROADDISTANCE;
		if (value > threshold) {
			idx = dest;
			res.SEARCH_TOTAL += value;
			res.SEARCH_COUNT += 1.0;
			continue;
		}

		dest = idx + ivec2(dir, +1); //Down
		value = texelFetch(map, dest, 0).CH_ROADDISTANCE;
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

	mediump vec4 current = texelFetch(src, pxIdx, 0);
	if (current.CH_ROADDISTANCE > 0.0) {

		highp vec2 left = search(src, 0.0, pxIdx, -1);
		highp vec2 right = search(src, 0.0, pxIdx, +1);

		if ( min(left.SEARCH_COUNT, right.SEARCH_COUNT) > MIN_SAMPLE_SIZE && abs(0.5 + left.SEARCH_COUNT - right.SEARCH_COUNT) < 1.0 ) { //Remove small sample (noise); Get center point, left == right or left == right - 1
			highp float total = left.SEARCH_TOTAL + right.SEARCH_TOTAL;
			highp float count = left.SEARCH_COUNT + right.SEARCH_COUNT;
			res.CH_ROADDISTANCE = total / count;
			res.CH_TARGETYCOORD = current.CH_TARGETYCOORD;
		}
	}

	result = res / 256.0f; //Normalize to RB8 format [0,1]
}
