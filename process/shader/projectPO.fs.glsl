in vec2 currentPos;
out vec4 result;

uniform sampler2D src;
uniform highp isampler2D roadmapT2;
uniform ivec4 mode;

#define CH_ROADMAP2_LOOKUP_P2O z
#define CH_ROADMAP2_LOOKUP_O2P w

#define MODE mode.x
#define MODE_P2O 1
#define MODE_O2P 2

void main() {
	ivec2 currentIdx = ivec2( vec2(textureSize(src, 0)) * currentPos );

	ivec2 sampleIdx = ivec2(0, 0);
	if (MODE == MODE_P2O)
		sampleIdx = ivec2(texelFetch(roadmapT2, currentIdx, 0).CH_ROADMAP2_LOOKUP_P2O, currentIdx.y);
	else if (MODE == MODE_O2P)
		sampleIdx = ivec2(texelFetch(roadmapT2, currentIdx, 0).CH_ROADMAP2_LOOKUP_O2P, currentIdx.y);

	result = texelFetch(src, sampleIdx, 0);
}
