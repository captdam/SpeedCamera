in highp vec2 pxPos;
out lowp vec4 result; //lowp for RGBA8 video

uniform lowp sampler2D src; //lowp for RGBA8 video
uniform mediump sampler2D roadmapT1;

/* Defined by client: STEP float */
/* defined by client: EDGE vec4 */
/* Defined by client: HORIZONTAL ^ VERTICAL */
/* Defined by client: SEARCH_DISTANCE float */

mediump vec2 pixelPerspSize(sampler2D map, mediump vec2 pos) {
	mediump vec4 self = texture(map, pos);
	mediump vec4 hori = pos.x > 0.5 ?
		textureLodOffset(map, pos, 0.0, ivec2(-1, 0)):
		textureLodOffset(map, pos, 0.0, ivec2(+1, 0));
	mediump vec4 vert = pos.y > 0.5 ?
		textureLodOffset(map, pos, 0.0, ivec2(0, -1)):
		textureLodOffset(map, pos, 0.0, ivec2(0, +1));
	return abs(vec2( hori.x - self.x , vert.y - self.y ));
}

void main() {
	lowp float det = 0.0;
	if (texture(src, pxPos).r > 0.0)
		det = 1.0;
	else {
		mediump ivec2 srcSize = textureSize(src, 0);
		mediump vec2 srcSizeF = vec2(srcSize);

		mediump vec2 pixelStep = vec2(STEP) / srcSizeF; //Search step, every step takes 2 pixels (textureGather takes 2*2 titles)
		
		#if defined(HORIZONTAL)
			mediump float limit = (SEARCH_DISTANCE / pixelPerspSize(roadmapT1, pxPos).x) / srcSizeF.x; //limit in pixels to NTC
			mediump float edgeLeft = max(EDGE[0], pxPos.x - limit), edgeRight = min(EDGE[1], pxPos.x + limit);
			
			lowp float foundLeft = 0.0, foundRight = 0.0; //Avoid using bool, stalls pipeline
			for (mediump vec2 pos = pxPos; pos.x > edgeLeft; pos.x -= pixelStep.x) {
				if (dot(textureGather(src, pos, 0), vec4(1.0)) > 0.0) {
					foundLeft = 0.7;
					break;
				}
			}
			for (mediump vec2 pos = pxPos; pos.x < edgeRight; pos.x += pixelStep.x) {
				if (dot(textureGather(src, pos, 0), vec4(1.0)) > 0.0) {
					foundRight = 0.7;
					break;
				}
			}
			det = foundLeft * foundRight; //0.49 if both side found

		#elif defined(VERTICAL)
			mediump float limit = (SEARCH_DISTANCE / pixelPerspSize(roadmapT1, pxPos).y) / srcSizeF.y;
			mediump float edgeUp = max(EDGE[2], pxPos.y - limit), edgeDown = min(EDGE[3], pxPos.y + limit);

			lowp float foundUp = 0.0, foundDown = 0.0;
			for (mediump vec2 pos = pxPos; pos.y >= edgeUp; pos.y -= pixelStep.y) {
				if (dot(textureGather(src, pos, 0), vec4(1.0)) > 0.0) {
					foundUp = 0.7;
					break;
				}
			}
			for (mediump vec2 pos = pxPos; pos.y <= edgeDown; pos.y += pixelStep.y) {
				if (dot(textureGather(src, pos, 0), vec4(1.0)) > 0.0) {
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
}
