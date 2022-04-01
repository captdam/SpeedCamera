in highp vec2 pxPos;
out mediump vec4 result; //Data, speed

uniform lowp sampler2D current; //lowp for enum
uniform lowp sampler2D previous; //lowp for enum
uniform mediump sampler2D roadmapT1;
uniform mediump sampler2D roadmapT2;
#define CH_ROADMAP1_POS_OY w
#define CH_ROADMAP2_SEARCHDUPDOWN xy

/* Defined by client: BIAS float */

void main() {
	mediump ivec2 videoSizeI = textureSize(current, 0);
	mediump vec2 videoSizeF = vec2(videoSizeI);

	mediump ivec2 pxIdx = ivec2(videoSizeF * pxPos);

	mediump float dis = 0.0;
	mediump float distanceUp = -1.0, distanceDown = -1.0; //Exported for debug

	if (texelFetch(current, pxIdx, 0).r == 1.0) { //==1.0 means edge
		highp float currentPos = texture(roadmapT1, pxPos).CH_ROADMAP1_POS_OY; //Roadmap may have different size, roadmap is linear, interpolation is OK
		mediump vec2 limitUpDown = texture(roadmapT2, pxPos).CH_ROADMAP2_SEARCHDUPDOWN; //NTC
		mediump ivec2 limitUpDownAbs = ivec2( limitUpDown * videoSizeF.y );
		mediump int limitUp = limitUpDownAbs.x;
		mediump int limitDown = limitUpDownAbs.y;

		for (mediump ivec2 idx = pxIdx; idx.y > limitUp; idx.y--) {
			if (texelFetch(previous, idx, 0).r > 0.0) { //>0.0 means object
				
//				ivec2 roadIdx = idx * textureSize(roadmapT1, 0) / textureSize(current, 0);
//				float roadPos = texelFetch(roadmapT1, roadIdx, 0).CH_ROADMAP1_POS_OY;

				mediump float roadPos = texture( roadmapT1 , vec2(idx)/videoSizeF ).CH_ROADMAP1_POS_OY;

				distanceUp = roadPos - currentPos;
				break;
			}
		}

		for (mediump ivec2 idx = pxIdx; idx.y < limitDown; idx.y++) {
			if (texelFetch(previous, idx, 0).r > 0.0) {
		
//				ivec2 roadIdx = idx * textureSize(roadmapT1, 0) / textureSize(current, 0);
//				float roadPos = texelFetch(roadmapT1, roadIdx, 0).CH_ROADMAP1_POS_OY;

				mediump float roadPos = texture( roadmapT1 , vec2(idx)/videoSizeF ).CH_ROADMAP1_POS_OY;

				distanceDown = currentPos - roadPos;
				break;
			}
		}

		if (distanceUp != -1.0 && distanceDown != -1.0) //Search success on both directions
			dis = min(distanceUp, distanceDown);
		else if (distanceUp != -1.0) //Search success on only one direction
			dis = distanceUp;
		else if (distanceDown != -1.0)
			dis = distanceDown;
	}

	result = vec4(min(dis * BIAS, 255.0), distanceUp, distanceDown, 1.0);
}
