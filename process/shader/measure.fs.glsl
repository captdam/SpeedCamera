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

//	mediump vec2 dis = vec2(0.0, pxIdx);
	mediump vec2 dis = vec2(0.0, 0.0);

	if (texelFetch(current, pxIdx, 0).r == DEST_THRESHOLD_CENEDGEJ) {
		mediump float currentPos = texture(roadmapT1, pxPos).y; //Roadmap may have different size, use NTC; roadmap is linear, interpolation sample is OK

		mediump float hintUp = 0.0, hintDown = 0.0;
		if (texelFetch(hint, pxIdx, 0).r < DEST_THRESHOLD_EDGE) { //Check if the object has move (current pixel in hint frame is not edge, but can be object)
			mediump ivec2 limitUpDown = ivec2( texture(roadmapT2, pxPos).xy * videoSizeF.y );
			for (mediump ivec2 idx = pxIdx; idx.y > limitUpDown[0]; idx.y--) {
				if (texelFetch(hint, idx, 0).r >= DEST_THRESHOLD_EDGE) {
					mediump float roadPos = texture( roadmapT1 , vec2(idx)/videoSizeF ).y;
					hintUp = roadPos - currentPos;
					break;
				}
			}
			for (mediump ivec2 idx = pxIdx; idx.y < limitUpDown[1]; idx.y++) {
				if (texelFetch(hint, idx, 0).r >= DEST_THRESHOLD_EDGE) {
					mediump float roadPos = texture( roadmapT1 , vec2(idx)/videoSizeF ).y;
					hintDown = currentPos - roadPos;
					break;
				}
			}
		}

		bool searchUp, searchDown;
		if ( hintUp != 0.0 && hintDown != 0.0 ) { //If both direction can reach object, use cloest direction
			if (hintUp < hintDown)
				searchUp = true;
			else
				searchDown = true;
		} else if (hintUp != 0.0) { //If only one direction can reach object, go that direction
			searchUp = true;
			searchDown = false;
		} else if (hintDown != 0.0) {
			searchUp = false;
			searchDown = true;
		} else { //None direction can reach object, no search
			searchUp = false;
			searchDown = false;
		}

		if (searchUp) {
			for (mediump ivec2 idx = pxIdx; idx.y > LIMIT_UP; idx.y--) {
				if (texelFetch(previous, idx, 0).r >= DEST_THRESHOLD_EDGE) {
					mediump float roadPos = texture( roadmapT1 , vec2(idx)/videoSizeF ).y;
					dis = vec2(roadPos - currentPos, idx.y);
					break;
				}
			}
		} else if (searchDown) {
			for (mediump ivec2 idx = pxIdx; idx.y < LIMIT_DOWN; idx.y++) {
				if (texelFetch(previous, idx, 0).r >= DEST_THRESHOLD_EDGE) {
					mediump float roadPos = texture( roadmapT1 , vec2(idx)/videoSizeF ).y;
					dis = vec2(currentPos - roadPos, idx.y);
					break;
				}
			}
		} else {
			dis = vec2(0.0, pxIdx);
		}
	}

	dis.CH_ROADDISTANCE = min(255.0, dis.CH_ROADDISTANCE * BIAS);
	result = dis;
}
