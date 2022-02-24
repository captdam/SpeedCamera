in vec2 pxPos;
out vec4 result;

uniform sampler2D current;
uniform sampler2D previous;
uniform sampler2D roadmapT1;
uniform highp isampler2D roadmapT2;
#define CH_ROADMAP1_POS_OY w
#define CH_ROADMAP2_SEARCHDISTANCE y

/* Defined by client: BIAS vec4(const, order1, 0.0, 0.0) */

void main() {
	ivec2 pxIdx = ivec2(vec2(textureSize(current, 0)) * pxPos);

	float absDistance = 0.0, upDistance = -1.0, downDistance = -1.0;

	if (texelFetch(current, pxIdx, 0).r == 1.0) {
		float currentPos = texelFetch(roadmapT1, pxIdx, 0).CH_ROADMAP1_POS_OY;
		int limit = texelFetch(roadmapT2, pxIdx, 0).CH_ROADMAP2_SEARCHDISTANCE;

		for (ivec2 idx = pxIdx; idx.y >= 0 && pxIdx.y - idx.y < limit; idx.y--) {
			if ( texelFetch(previous, idx, 0).r > 0.0 ) {
				float upPos = texelFetch(roadmapT1, idx, 0).CH_ROADMAP1_POS_OY;
				upDistance = upPos - currentPos;
				break;
			}
		}
		for (ivec2 idx = pxIdx; idx.y < textureSize(previous, 0).y && idx.y - pxIdx.y < limit; idx.y++) {
			if ( texelFetch(previous, idx, 0).r > 0.0 ) {
				float downPos = texelFetch(roadmapT1, idx, 0).CH_ROADMAP1_POS_OY;
				downDistance = currentPos - downPos;
				break;
			}
		}

		if (upDistance != -1.0 && downDistance != -1.0) //Search success on both directions
			absDistance = min(upDistance, downDistance);
		else if (upDistance != -1.0) //Search success on only one direction
			absDistance = upDistance;
		else if (downDistance != -1.0)
			absDistance = downDistance;
	}

	vec3 d = vec3(absDistance, upDistance, downDistance) * vec3(BIAS[1]) + vec3(BIAS[0]);
	result = vec4(d, 1.0);
}
