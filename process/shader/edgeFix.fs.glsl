in vec2 pxPos;
out vec4 result;

uniform sampler2D edgemap;
#define CH_EDGEMAP_CURRENT r
#define CH_EDGEMAP_PREVIOUS g

/* Defined by client: ivec4 tolerance */
#define CURRENT_TOLERANCE tolerance.x
#define PREVIOUS_TOLERANCE tolerance.y

void main() {
	ivec2 pxIdx = ivec2(vec2(textureSize(edgemap, 0)) * pxPos);

	float currentEdge = 0.0, previousEdge = 0.0;
	for (int i = 0; i < CURRENT_TOLERANCE + 1; i++) {
		if (
			texelFetch(edgemap, pxIdx + ivec2(+i,  0), 0).CH_EDGEMAP_CURRENT > 0.0
		||	texelFetch(edgemap, pxIdx + ivec2(-i,  0), 0).CH_EDGEMAP_CURRENT > 0.0
//		||	texelFetch(edgemap, pxIdx + ivec2(+i, +1), 0).CH_EDGEMAP_CURRENT > 0.0
//		||	texelFetch(edgemap, pxIdx + ivec2(-i, +1), 0).CH_EDGEMAP_CURRENT > 0.0
//		||	texelFetch(edgemap, pxIdx + ivec2(+i, -1), 0).CH_EDGEMAP_CURRENT > 0.0
//		||	texelFetch(edgemap, pxIdx + ivec2(-i, -1), 0).CH_EDGEMAP_CURRENT > 0.0
		) {
			currentEdge = 1.0;
			break;
		}
	}
	for (int i = 0; i < PREVIOUS_TOLERANCE + 1; i++) {
		if (
			texelFetch(edgemap, pxIdx + ivec2(+i,  0), 0).CH_EDGEMAP_PREVIOUS > 0.0
		||	texelFetch(edgemap, pxIdx + ivec2(-i,  0), 0).CH_EDGEMAP_PREVIOUS > 0.0
//		||	texelFetch(edgemap, pxIdx + ivec2(+i, +1), 0).CH_EDGEMAP_PREVIOUS > 0.0
//		||	texelFetch(edgemap, pxIdx + ivec2(-i, +1), 0).CH_EDGEMAP_PREVIOUS > 0.0
//		||	texelFetch(edgemap, pxIdx + ivec2(+i, -1), 0).CH_EDGEMAP_PREVIOUS > 0.0
//		||	texelFetch(edgemap, pxIdx + ivec2(-i, -1), 0).CH_EDGEMAP_PREVIOUS > 0.0
		) {
			previousEdge = 1.0;
			break;
		}
	}

	result = vec4(currentEdge, previousEdge, 0.0, step(0.0, max(currentEdge, previousEdge)));
}
