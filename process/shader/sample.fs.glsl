in vec2 pxPos;
out vec4 result;

uniform sampler2D src;

#define SEARCH_TOTAL x
#define SEARCH_COUNT y

vec2 search(sampler2D map, float threshold, ivec2 start, int dir) {
	vec2 res = vec2(0.0);
	ivec2 idx = start;
	while (true) {
		ivec2 dest;
		float value;

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

		break;
	}
	return res;
}

void main() {
	ivec2 pxIdx = ivec2(vec2(textureSize(src, 0)) * pxPos);

	float res = 0.0;

	float current = texelFetch(src, pxIdx, 0).r;
	if (current > 0.0) {
		vec2 left = search(src, 0.0, pxIdx, -1);
		vec2 right = search(src, 0.0, pxIdx, +1);

		if (left.SEARCH_COUNT == right.SEARCH_COUNT || left.SEARCH_COUNT == right.SEARCH_COUNT + 1.0) { //Get center point
			float total = left.SEARCH_TOTAL + right.SEARCH_TOTAL;
			float count = left.SEARCH_COUNT + right.SEARCH_COUNT;
			res = total / count;
		}
	}

	result = vec4(res, 0.0, 0.0, 1.0);
}
