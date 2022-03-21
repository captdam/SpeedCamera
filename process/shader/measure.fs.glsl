in vec2 pxPos;
out vec4 result;

uniform sampler2D current;
uniform sampler2D previous;
uniform sampler2D roadmapT1;
uniform highp isampler2D roadmapT2;
#define CH_ROADMAP1_POS_OY w
#define CH_ROADMAP2_SEARCHDUP x
#define CH_ROADMAP2_SEARCHDOWN y

/* Defined by client: BIAS float */

void main() {
	ivec2 pxIdx = ivec2(vec2(textureSize(current, 0)) * pxPos);

	float dis = 0.0;

	if (texelFetch(current, pxIdx, 0).r == 1.0) { //==1.0 means edge
		float currentPos = texture(roadmapT1, pxPos).CH_ROADMAP1_POS_OY; //roadmap may have different size, interpolation is OK

		int limitUp = texture(roadmapT2, pxPos).CH_ROADMAP2_SEARCHDUP;
		int limitDown = texture(roadmapT2, pxPos).CH_ROADMAP2_SEARCHDOWN;

		float distanceUp = -1.0;
		for (ivec2 idx = pxIdx; idx.y >= limitUp; idx.y--) {
			if (texelFetch(previous, idx, 0).r > 0.0) { //>0.0 means object
				
//				ivec2 roadIdx = idx * textureSize(roadmapT1, 0) / textureSize(current, 0);
//				float roadPos = texelFetch(roadmapT1, roadIdx, 0).CH_ROADMAP1_POS_OY;

				float roadPos = texture( roadmapT1 , vec2(idx) / vec2(textureSize(current,0)) ).CH_ROADMAP1_POS_OY;

				distanceUp = roadPos - currentPos;
				break;
			}
		}

		float distanceDown = -1.0;
		for (ivec2 idx = pxIdx; idx.y <= limitDown; idx.y++) {
			if (texelFetch(previous, idx, 0).r > 0.0) {
		
//				ivec2 roadIdx = idx * textureSize(roadmapT1, 0) / textureSize(current, 0);
//				float roadPos = texelFetch(roadmapT1, roadIdx, 0).CH_ROADMAP1_POS_OY;

				float roadPos = texture( roadmapT1 , vec2(idx) / vec2(textureSize(current,0)) ).CH_ROADMAP1_POS_OY;

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

	result = vec4(min(dis * BIAS, 255.0), 0.0, 0.0, 1.0);
}
