in highp vec2 pxPos;
out highp vec4 result; //highp for general data

uniform highp sampler2D src; //highp for general data
uniform mediump sampler2D roadmapT2;

/* Defined by client: P2O || O2P */

void main() {
	/* Note: 
	* Use texelFetch() instead of texture(): 
	* When data is stored in src, and when the data is non-linear, we should directly fetch that data without applying any filter on it. 
	* We can to use int16 instead of float16 for better accuracy. 
	*/
	
	mediump ivec2 srcSize = textureSize(src, 0);
	mediump vec2 srcSizeF = vec2(srcSize);

	mediump ivec2 projIdx = ivec2( srcSizeF * pxPos );

	#if defined(P2O)
		projIdx.x = int( srcSizeF.x * texture(roadmapT2, pxPos).z ); //Road surface is linear, so does the projection table, ok to use texture sampling here
	#elif defined(O2P)
		projIdx.x = int( srcSizeF.x * texture(roadmapT2, pxPos).w );
	#endif
		//Default: projIdx.x not changed, no projection

	result = texelFetch(src, projIdx, 0);
}
