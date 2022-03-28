in vec2 pxPos;
out vec4 result;

uniform sampler2D src;
uniform sampler2D roadmapT1;

/* Defined by client: HORIZONTAL ^ VERTICAL */
/* Defined by client: SEARCH_DISTANCE float */

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
	float det = 0.0;
	if (texture(src, pxPos).r > 0.0)
		det = 1.0;
	else {
		ivec2 srcSizeI = textureSize(src, 0);
		vec2 srcSizeF = vec2(srcSizeI);

		vec2 offset8 = vec2(0.125, 0);
		float roadWidth8 = pxPos.x < 0.5 ? //Size of 1/8 NTC
			( texture(roadmapT1, pxPos + offset8).x - texture(roadmapT1, pxPos          ).x ):
			( texture(roadmapT1, pxPos          ).x - texture(roadmapT1, pxPos - offset8).x );
		float pWidth = roadWidth8 * 8.0 / srcSizeF.x; //Pixel width in src
		int limit = int(ceil( SEARCH_DISTANCE / pWidth ));

		ivec2 pxIdx = ivec2(srcSizeF * pxPos);
		#if defined(HORIZONTAL)
			
			float foundLeft = 0.0, foundRight = 0.0; //Avoid using bool, stalls pipeline
			int edgeLeft = max(0, pxIdx.x - limit), edgeRight = min(srcSizeI.x - 1, pxIdx.x + limit);
			
			for (ivec2 idx = pxIdx; idx.x >= edgeLeft; idx.x--) {
				if (texelFetch(src, idx, 0).r > 0.0) {
					foundLeft = 0.7;
					break;
				}
			}
			for (ivec2 idx = pxIdx; idx.x <= edgeRight; idx.x++) {
				if (texelFetch(src, idx, 0).r > 0.0) {
					foundRight = 0.7;
					break;
				}
			}
			det = foundLeft * foundRight; //0.49 if both side found

		#elif defined(VERTICAL)

			float foundUp = 0.0, foundDown = 0.0;
			int edgeUp = max(0, pxIdx.y - limit), edgeDown = min(srcSizeI.y - 1, pxIdx.y + limit);
			
			for (ivec2 idx = pxIdx; idx.y >= edgeUp; idx.y--) {
				if (texelFetch(src, idx, 0).r > 0.0) {
					foundUp = 0.7;
					break;
				}
			}
			for (ivec2 idx = pxIdx; idx.y <= edgeDown; idx.y++) {
				if (texelFetch(src, idx, 0).r > 0.0) {
					foundDown = 0.7;
					break;
				}
			}
			det = foundUp * foundDown;

		#else
			#error(Must define one of HORIZONTAL and VERTICAL exclusively (XOR))
		#endif
	}

	result = vec4(det, 0.0, 0.0, 1.0);
}
