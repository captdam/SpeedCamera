in vec2 pxPos;
out vec4 result;

uniform sampler2D src;

struct sr_t{float total; int count;};
sr_t search(sampler2D map, float threshold, ivec2 start, int dir) {
	sr_t res = {0.0, 0};
	ivec2 idx = start;
	while (true) {
		ivec2 dest;
		float value;

		dest = idx + ivec2(dir, 0); //Middle
		value = texelFetch(map, dest, 0).r;
		if (value > threshold) {
			idx = dest;
			res.total += value;
			res.count++;
			continue;
		}

		dest = idx + ivec2(dir, -1); //Up
		value = texelFetch(map, dest, 0).r;
		if (value > threshold) {
			idx = dest;
			res.total += value;
			res.count++;
			continue;
		}

		dest = idx + ivec2(dir, +1); //Down
		value = texelFetch(map, dest, 0).r;
		if (value > threshold) {
			idx = dest;
			res.total += value;
			res.count++;
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
		sr_t left = search(src, 0.0, pxIdx, -1);
		sr_t right = search(src, 0.0, pxIdx, +1);

		if (left.count == right.count || left.count == right.count + 1) { //Get center point
			float total = left.total + right.total;
			int count = left.count + right.count;
			res = total / float(count);
		}
	}

	result = vec4(res, 0.0, 0.0, 1.0);
}
