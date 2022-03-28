in highp vec2 pxPos;
out lowp vec4 result; //lowp for RGBA8 video

uniform lowp sampler2D src; //lowp for RGBA8 video
uniform mediump sampler2D roadmapT1;
uniform mediump sampler2D roadmapT3;

/* Defined by client: HORIZONTAL ^ VERTICAL */
/* Defined by client: SEARCH_DISTANCE float */

void main() {
	lowp float det = 0.0;
	if (texture(src, pxPos).r > 0.0)
		det = 1.0;
	else {
		mediump ivec2 srcSize = textureSize(src, 0);
		mediump vec2 srcSizeF = vec2(srcSize);

		mediump vec2 offset8 = vec2(0.125, 0);
		mediump float roadWidth8 = pxPos.x < 0.5 ? //Size of 1/8 NTC
			( texture(roadmapT1, pxPos + offset8).x - texture(roadmapT1, pxPos          ).x ):
			( texture(roadmapT1, pxPos          ).x - texture(roadmapT1, pxPos - offset8).x );
		mediump float pWidth = roadWidth8 * 8.0 / srcSizeF.x; //Pixel width in src
		mediump int limit = int(ceil( SEARCH_DISTANCE / pWidth ));

		mediump ivec2 pxIdx = ivec2( srcSizeF * pxPos );
		#if defined(HORIZONTAL)
			
			lowp float foundLeft = 0.0, foundRight = 0.0; //Avoid using bool, stalls pipeline
			mediump int edgeLeft = max(0, pxIdx.x - limit), edgeRight = min(srcSize.x - 1, pxIdx.x + limit);
			
			for (mediump ivec2 idx = pxIdx; idx.x >= edgeLeft; idx.x--) {
				if (texelFetch(src, idx, 0).r > 0.0) {
					foundLeft = 0.7;
					break;
				}
			}
			for (mediump ivec2 idx = pxIdx; idx.x <= edgeRight; idx.x++) {
				if (texelFetch(src, idx, 0).r > 0.0) {
					foundRight = 0.7;
					break;
				}
			}
			det = foundLeft * foundRight; //0.49 if both side found

		#elif defined(VERTICAL)

			lowp float foundUp = 0.0, foundDown = 0.0;
			mediump int edgeUp = max(0, pxIdx.y - limit), edgeDown = min(srcSize.y - 1, pxIdx.y + limit);
			
			for (mediump ivec2 idx = pxIdx; idx.y >= edgeUp; idx.y--) {
				if (texelFetch(src, idx, 0).r > 0.0) {
					foundUp = 0.7;
					break;
				}
			}
			for (mediump ivec2 idx = pxIdx; idx.y <= edgeDown; idx.y++) {
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

	result = vec4(det);
	result.a += 0.000001 * texture(roadmapT3, pxPos).a; //placeholder
}
