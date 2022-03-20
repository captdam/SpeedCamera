in vec2 pxPos;
out vec4 result;

uniform sampler2D src;
uniform highp isampler2D roadmapT2;

/* Defined by client: P2O || O2P */

void main() {
	ivec2 pxIdx = ivec2(vec2(textureSize(src, 0)) * pxPos);
	 
	#if defined(P2O)
		/* Note: 
		* Use texture with normalized coord instead of texelFetch with absolute coord, 
		* roadmap may have different size than the video.
		*/
		pxIdx.x = texture(roadmapT2, pxPos).z * textureSize(src, 0).x / textureSize(roadmapT2, 0).x;
	#elif defined(O2P)
		pxIdx.x = texture(roadmapT2, pxPos).w * textureSize(src, 0).x / textureSize(roadmapT2, 0).x;
	#endif
		//Default: pxIdx.x not changed, no projection

	result = texelFetch(src, pxIdx, 0);
	/* Note: 
	* Use texelFetch() to avoid sample when project. 
	* When data is stored in texture, We should directly fetch that data without any processing on it. 
	*/
}
