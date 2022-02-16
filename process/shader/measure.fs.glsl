in vec2 pxPos;
out vec4 result;

uniform sampler2D current;
uniform sampler2D previous;
uniform sampler2D roadmapT1;
uniform highp isampler2D roadmapT2;
#define CH_ROADMAP1_POS_OY w
#define CH_ROADMAP2_SEARCHDISTANCE y

void main() {
	ivec2 pxIdx = ivec2(vec2(textureSize(current, 0)) * pxPos);

	float distance = 0.0;
		float upDistance = 0.0, downDistance = 0.0;

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

		distance = min(upDistance, downDistance);
	}

	result = vec4(texelFetch(current, pxIdx, 0).r, upDistance, downDistance, 1.0);
}
