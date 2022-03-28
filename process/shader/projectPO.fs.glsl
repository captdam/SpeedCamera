in vec2 pxPos;
out vec4 result;

uniform sampler2D src;
uniform sampler2D roadmapT2;

/* Defined by client: P2O || O2P */

void main() {
	vec2 projPos = pxPos;

	#if defined(P2O)
		projPos.x = texture(roadmapT2, pxPos).z; //Road surface is linear, ok to use texture sampling
	#elif defined(O2P)
		projPos.x = texture(roadmapT2, pxPos).w;
	#endif
		//Default: projPos.x not changed, no projection

	result = texelFetch(src, ivec2(vec2(textureSize(src, 0)) * projPos), 0);
	/* Note: 
	* Use texelFetch() to avoid sample when project. 
	* When data is stored in src, and when the data is non-linear, we should directly fetch that data without applying any filter on it. 
	*/
}
