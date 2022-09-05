in highp vec2 pxPos;
out mediump vec2 result; //Data: vec2(speed, target_yCoord_abs)
#define CH_ROADDISTANCE x
#define CH_TARGETYCOORD y

uniform lowp sampler2D current; //lowp for enum
uniform lowp sampler2D hint; //lowp for enum
uniform lowp sampler2D previous; //lowp for enum
uniform mediump sampler2D roadmapT1;
uniform mediump sampler2D roadmapT2;

/* Defined by client: BIAS float */
/* Defined by client: LIMIT_UP int && LIMIT_DOWN int*/

#define DEST_THRESHOLD_OBJ 0.3
#define DEST_THRESHOLD_EDGE 0.6
#define DEST_THRESHOLD_CENEDGEJ 1.0

void main() {
	mediump ivec2 videoSizeI = textureSize(current, 0);
	mediump vec2 videoSizeF = vec2(videoSizeI);

	mediump ivec2 pxIdx = ivec2(videoSizeF * pxPos);

	mediump float dis = 0.0;
	mediump int targetY = pxIdx.y;

	if (texelFetch(current, pxIdx, 0).r == DEST_THRESHOLD_CENEDGEJ) {
		mediump float currentPos = texture(roadmapT1, pxPos).y; //Roadmap may have different size, use NTC; roadmap is linear, interpolation sample is OK

		mediump float hintUpRoad = 0.0, hintDownRoad = 0.0;
		mediump int hintUpScreen = pxIdx.y, hintDownScreen = pxIdx.y;
		if (texelFetch(hint, pxIdx, 0).r < DEST_THRESHOLD_EDGE) { //Check if the object has move (current pixel in hint frame is not edge, but can be object)
			mediump ivec2 limitUpDown = ivec2( texture(roadmapT2, pxPos).xy * videoSizeF.y );
			for (mediump ivec2 idx = pxIdx; idx.y > limitUpDown[0]; idx.y--) {
				if (texelFetch(hint, idx, 0).r >= DEST_THRESHOLD_EDGE) {
					mediump float roadPos = texture( roadmapT1 , vec2(idx)/videoSizeF ).y;
					hintUpRoad = roadPos - currentPos;
					hintUpScreen = idx.y;
					break;
				}
			}
			for (mediump ivec2 idx = pxIdx; idx.y < limitUpDown[1]; idx.y++) {
				if (texelFetch(hint, idx, 0).r >= DEST_THRESHOLD_EDGE) {
					mediump float roadPos = texture( roadmapT1 , vec2(idx)/videoSizeF ).y;
					hintDownRoad = currentPos - roadPos;
					hintDownScreen = idx.y;
					break;
				}
			}
		}

		bool searchUp, searchDown;
		mediump int hintTargetY;
		if ( hintUpRoad != 0.0 && hintDownRoad != 0.0 ) { //If both direction can reach object, use cloest direction
			if (hintUpRoad < hintDownRoad) {
				searchUp = true;
				searchDown = false;
				hintTargetY = hintUpScreen;
			} else {
				searchDown = true;
				searchUp = false;
				hintTargetY = hintDownScreen;
			}
		} else if (hintUpRoad != 0.0) { //If only one direction can reach object, go that direction
			searchUp = true;
			searchDown = false;
			hintTargetY = hintUpScreen;
		} else if (hintDownRoad != 0.0) {
			searchUp = false;
			searchDown = true;
			hintTargetY = hintDownScreen;
		} else { //None direction can reach object, no search
			searchUp = false;
			searchDown = false;
			hintTargetY = pxIdx.y;
		}

		mediump int searchTargetY;
		if (searchUp) {
			for (mediump ivec2 idx = pxIdx; idx.y > LIMIT_UP; idx.y--) {
				if (texelFetch(previous, idx, 0).r >= DEST_THRESHOLD_EDGE) {
					mediump float roadPos = texture( roadmapT1 , vec2(idx)/videoSizeF ).y;
					dis = roadPos - currentPos;
					searchTargetY = idx.y;
					break;
				}
			}
		} else if (searchDown) {
			for (mediump ivec2 idx = pxIdx; idx.y < LIMIT_DOWN; idx.y++) {
				if (texelFetch(previous, idx, 0).r >= DEST_THRESHOLD_EDGE) {
					mediump float roadPos = texture( roadmapT1 , vec2(idx)/videoSizeF ).y;
					dis = currentPos - roadPos;
					searchTargetY = idx.y;
					break;
				}
			}
		}

		targetY = hintTargetY;
//		targetY = searchTargetY;
	}

	dis = min(255.0, dis * BIAS);
	result = vec2(dis, abs(pxIdx.y-targetY));
}
