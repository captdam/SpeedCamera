@VS

layout (location = 0) in highp vec2 meshROI;
out mediump vec2 pxPos;

void main() {
	gl_Position = vec4(meshROI.x * 2.0 - 1.0, meshROI.y * 2.0 - 1.0, 0.0, 1.0);
	pxPos = meshROI.xy;
}

@FS

uniform lowp sampler2D current; //lowp for enum
uniform lowp sampler2D hint; //lowp for enum
uniform lowp sampler2D previous; //lowp for enum
uniform mediump sampler2DArray roadmap;
uniform mediump float bias;

in mediump vec2 pxPos;
out lowp vec2 result; //Data: vec2(speed, target_yCoord_abs)

#define DEST_THRESHOLD_OBJ 0.3
#define DEST_THRESHOLD_EDGE 0.6
#define DEST_THRESHOLD_CENEDGE 1.0

mediump vec4 getLimit() {
	mediump vec4 limitGatherUp = textureGather(roadmap, vec3(pxPos, 1.0), 0); //Channel x for up limit
	mediump vec4 limitGatherDown = textureGather(roadmap, vec3(pxPos, 1.0), 1); //Channel y for down limit
	mediump float upHint = max(limitGatherUp[3], limitGatherUp[1]); //Up hint should be closer to current y than up search (search upwards (-y), hint has greater y)
	mediump float upSearch = min(limitGatherUp[3], limitGatherUp[1]);
	mediump float downHint = min(limitGatherDown[3], limitGatherDown[1]); //Search downwards (+y), hint has smaller y
	mediump float downSearch = max(limitGatherDown[3], limitGatherDown[1]);
	// Note:
	// the limit on (x,y) and (x,y+1) is similar, an extra compute to determine y value (to choose from _j0 or _j1) is redundant
	return vec4(upHint, downHint, upSearch, downSearch);
}

void main() {
	mediump ivec2 videoSizeI = textureSize(current, 0);
	mediump vec2 videoSizeF = vec2(videoSizeI);

	mediump ivec2 pxIdx = ivec2(videoSizeF * pxPos);

	mediump ivec4 limit = ivec4(getLimit() * videoSizeF.y);

	mediump float displaceRoad = 0.0;
	mediump int displaceScreen = pxIdx.y;

	if (texelFetch(current, pxIdx, 0).r == DEST_THRESHOLD_CENEDGE) {
		mediump float currentPos = texture(roadmap, vec3(pxPos, 0.0)).y; //Roadmap may have different size, use NTC; roadmap is linear, interpolation sample is OK

		mediump float hintUpRoad = 0.0, hintDownRoad = 0.0;
		mediump int hintUpScreen = pxIdx.y, hintDownScreen = pxIdx.y;
		if (texelFetch(hint, pxIdx, 0).r < DEST_THRESHOLD_EDGE) { //If this pixel is not edge (can be object or road), then the object has move
			for (mediump ivec2 idx = pxIdx; idx.y > limit.x; idx.y--) {
				if (texelFetch(hint, idx, 0).r >= DEST_THRESHOLD_EDGE) {
					mediump float roadPos = texture( roadmap , vec3(vec2(idx)/videoSizeF, 0.0) ).y;
					hintUpRoad = abs(currentPos - roadPos);
					hintUpScreen = idx.y;
					break;
				}
			}
			for (mediump ivec2 idx = pxIdx; idx.y < limit.y; idx.y++) {
				if (texelFetch(hint, idx, 0).r >= DEST_THRESHOLD_EDGE) {
					mediump float roadPos = texture( roadmap , vec3(vec2(idx)/videoSizeF, 0.0) ).y;
					hintDownRoad = abs(currentPos - roadPos);
					hintDownScreen = idx.y;
					break;
				}
			}
		}

		bool searchUp = false, searchDown = false;
		mediump int hintTargetY = pxIdx.y;
		if ( hintUpScreen != pxIdx.y && hintDownScreen != pxIdx.y ) { //If both direction can reach object, use cloest direction
			if (hintUpRoad < hintDownRoad) {
				searchUp = true;
				hintTargetY = hintUpScreen;
			} else {
				searchDown = true;
				hintTargetY = hintDownScreen;
			}
		} else if (hintUpScreen != pxIdx.y) { //If only one direction can reach object, go that direction
			searchUp = true;
			hintTargetY = hintUpScreen;
		} else if (hintDownScreen != pxIdx.y) {
			searchDown = true;
			hintTargetY = hintDownScreen;
		} else { //None direction can reach object, no search
			// Kepp both searchUp and searchDown flase, keep hintTargetY current pixel y
		}

		mediump int searchTargetY = pxIdx.y;
		if (searchUp) {
			for (mediump ivec2 idx = pxIdx - ivec2(0, 1); idx.y > limit.z; idx.y--) {
				if (texelFetch(previous, idx, 0).r >= DEST_THRESHOLD_EDGE) {
					mediump float roadPos = texture( roadmap , vec3(vec2(idx)/videoSizeF, 0.0) ).y;
					displaceRoad = abs(currentPos - roadPos);
					searchTargetY = idx.y;
					break;
				}
			}
		} else if (searchDown) {
			for (mediump ivec2 idx = pxIdx + ivec2(0, 1); idx.y < limit.w; idx.y++) {
				if (texelFetch(previous, idx, 0).r >= DEST_THRESHOLD_EDGE) {
					mediump float roadPos = texture( roadmap , vec3(vec2(idx)/videoSizeF, 0.0) ).y;
					displaceRoad = abs(currentPos - roadPos);
					searchTargetY = idx.y;
					break;
				}
			}
		}

		displaceScreen = hintTargetY;
//		displaceScreen = searchTargetY;
	}

	result = vec2(
		displaceRoad * bias / 255.0, //Normalize to [0,1], offset at 0.0
		0.5 + float(displaceScreen - pxIdx.y) / 127.0 //Normalize to [1,0], offset at 0.5
	);
}
