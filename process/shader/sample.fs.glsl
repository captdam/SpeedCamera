in highp vec2 pxPos;
out highp vec4 result; //Data, speed

uniform sampler2D src; //Data, speed

#define SEARCH_TOTAL x
#define SEARCH_COUNT y

highp vec2 search(highp sampler2D map, highp float threshold, mediump ivec2 start, lowp int dir) {
	highp vec2 res = vec2(0.0);
	mediump ivec2 idx = start;
	while (true) {
		mediump ivec2 dest;
		highp float value;

		dest = idx + ivec2(dir, 0); //Middle
		value = texelFetch(map, dest, 0).r;
		if (value > threshold) {
			idx = dest;
			res.SEARCH_TOTAL += value;
			res.SEARCH_COUNT += 1.0;
			continue;
		}

		dest = idx + ivec2(dir, -1); //Up
		value = texelFetch(map, dest, 0).r;
		if (value > threshold) {
			idx = dest;
			res.SEARCH_TOTAL += value;
			res.SEARCH_COUNT += 1.0;
			continue;
		}

		dest = idx + ivec2(dir, +1); //Down
		value = texelFetch(map, dest, 0).r;
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

	highp float res = 0.0;

	if (texelFetch(src, pxIdx, 0).r > 0.0) {
		highp vec2 left = search(src, 0.0, pxIdx, -1);
		highp vec2 right = search(src, 0.0, pxIdx, +1);

		if (abs(0.5 + left.SEARCH_COUNT - right.SEARCH_COUNT) < 1.0) { //Get center point, left == right or left == right - 1
			highp float total = left.SEARCH_TOTAL + right.SEARCH_TOTAL;
			highp float count = left.SEARCH_COUNT + right.SEARCH_COUNT;
			res = total / count;
		}
	}

	result = vec4(res);
}
