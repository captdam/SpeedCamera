in vec2 pxPos;
out vec4 result;

uniform sampler2D edgemap;
#define CH_EDGEMAP_CURRENT r
#define CH_EDGEMAP_PREVIOUS g

uniform sampler2D roadmapT1;
uniform highp isampler2D roadmapT2;
#define CH_ROADMAP1_POS w
#define CH_ROADMAP2_SEARCH y

/* Defined by client: vec4 bias */
#define CH_BIAS_OFFSET x
#define CH_BIAS_FIRSTORDER y

void main() {
	ivec2 pxIdx = ivec2(vec2(textureSize(edgemap, 0)) * pxPos);

	float speed = 0.0;

	if (texelFetch(edgemap, pxIdx, 0).CH_EDGEMAP_CURRENT > 0.0) {
		float currentPos = texelFetch(roadmapT1, pxIdx, 0).CH_ROADMAP1_POS;
		int searchLimit = texelFetch(roadmapT2, pxIdx, 0).CH_ROADMAP2_SEARCH;

		float upDistance = -1.0, downDistance = -1.0;
		for (ivec2 destIdx = pxIdx; destIdx.y > pxIdx.y - searchLimit && destIdx.y > 0; destIdx += ivec2(0,-1)) {
			if (texelFetch(edgemap, destIdx, 0).CH_EDGEMAP_PREVIOUS > 0.0) {
				upDistance = abs( texelFetch(roadmapT1, destIdx, 0).CH_ROADMAP1_POS - currentPos );
				break;
			}
		}
		for (ivec2 destIdx = pxIdx; destIdx.y < pxIdx.y + searchLimit && destIdx.y < textureSize(edgemap, 0).y; destIdx += ivec2(0,+1)) {
			if (texelFetch(edgemap, destIdx, 0).CH_EDGEMAP_PREVIOUS > 0.0) {
				downDistance = abs( texelFetch(roadmapT1, destIdx, 0).CH_ROADMAP1_POS - currentPos );
				break;
			}
		}

		float d = 0.0;
		if (upDistance >= 0.0 && downDistance >= 0.0)
			d = min(upDistance, downDistance);
		else if (upDistance >= 0.0)
			d = upDistance;
		else if (downDistance >= 0.0)
			d = downDistance;
		speed = d * bias.CH_BIAS_FIRSTORDER + bias.CH_BIAS_OFFSET;
	}

	result = vec4(speed, 0.0, 0.0, speed);
}
