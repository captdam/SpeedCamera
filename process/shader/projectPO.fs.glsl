in vec2 pxPos;
out vec4 result;

uniform sampler2D src;
uniform highp isampler2D roadmapT2;

/* Defined by client: P2O || O2P */

#define CH_ROADMAP2_LOOKUP_P2O z
#define CH_ROADMAP2_LOOKUP_O2P w

void main() {
	ivec2 pxIdx = ivec2(vec2(textureSize(src, 0)) * pxPos);

	ivec2 sampleIdx = pxIdx; //Default - no projection if client didn't define mode
	#if defined(P2O)
		sampleIdx.x = texelFetch(roadmapT2, pxIdx, 0).CH_ROADMAP2_LOOKUP_P2O;
	#elif defined(O2P)
		sampleIdx.x = texelFetch(roadmapT2, pxIdx, 0).CH_ROADMAP2_LOOKUP_O2P;
	#endif

	result = texelFetch(src, sampleIdx, 0);
}
