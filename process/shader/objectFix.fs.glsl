in vec2 pxPos;
out vec4 result;

uniform sampler2D src;
uniform sampler2D roadmapT1;

/* Defined by client: HORIZONTAL ^ VERTICAL */
/* Defined by client: SEARCH_DISTANCE float */

float getPixelWidth(sampler2D road, ivec2 pos) {
	ivec2 npos = pos + ivec2(1,0);
	npos.x = npos.x % textureSize(road, 0).x;
	return abs( texelFetch(road, npos, 0).x - texelFetch(road, pos, 0).x );
}

bool searchLeft(sampler2D map, ivec2 start, int limit) {
	for (ivec2 idx = start; idx.x >= 0 && start.x - idx.x < limit; idx.x--) {
		if (texelFetch(map, idx, 0).r > 0.0)
			return true;
	}
	return false;
}

bool searchRight(sampler2D map, ivec2 start, int limit) {
	for (ivec2 idx = start; idx.x < textureSize(map, 0).x && idx.x - start.x < limit; idx.x++) {
		if (texelFetch(map, idx, 0).r > 0.0)
			return true;
	}
	return false;
}

bool searchUp(sampler2D map, ivec2 start, int limit) {
	for (ivec2 idx = start; idx.y >= 0 && start.y - idx.y < limit; idx.y--) {
		if (texelFetch(map, idx, 0).r > 0.0)
			return true;
	}
	return false;
}

bool searchDown(sampler2D map, ivec2 start, int limit) {
	for (ivec2 idx = start; idx.y < textureSize(map, 0).y && idx.y - start.y < limit; idx.y++) {
		if (texelFetch(map, idx, 0).r > 0.0)
			return true;
	}
	return false;
}

void main() {
	ivec2 pxIdx = ivec2(vec2(textureSize(src, 0)) * pxPos);

	float pWidth = getPixelWidth(roadmapT1, pxIdx);
	int limit = int(ceil( SEARCH_DISTANCE / pWidth ));

	float det = 0.0;
	if (texelFetch(src, pxIdx, 0).r > 0.0)
		det = 1.0;
	else {
		#if defined(HORIZONTAL)
			if ( searchLeft(src, pxIdx, limit) && searchRight(src, pxIdx, limit) )
				det = 0.5;
		#elif defined(VERTICAL)
			if ( searchUp(src, pxIdx, limit) && searchDown(src, pxIdx, limit) )
				det = 0.5;
		#else
			#error(Must define one of HORIZONTAL and VERTICAL exclusively (XOR))
		#endif
	}

	result = vec4(det);
}
