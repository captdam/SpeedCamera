@VS

layout (location = 0) in highp vec2 meshROI;
out mediump vec2 pxPos;

void main() {
	gl_Position = vec4(meshROI.x * 2.0 - 1.0, meshROI.y * 2.0 - 1.0, 0.0, 1.0);
	pxPos = meshROI.xy;
}

@FS

uniform lowp sampler2D src; //lowp for RGBA8 video

in mediump vec2 pxPos;
out lowp vec4 result; //lowp for RGBA8 video

//#define MONO Get gray-scale result
//#define BINARY 0.5 Get black/white image
//#define BINARY vec4(0.3, 0.4, 0.5, -0.1) //Get black/white image, use different threshold for different channels

void main() {
	mediump ivec2 srcSize = textureSize(src, 0);
	mediump vec2 srcSizeF = vec2(srcSize);
	mediump ivec2 pxIdx = ivec2( srcSizeF * pxPos );

	//During accum, may excess lowp range, especially for edge filter when accum may get negative
	mediump vec4 accum = texelFetchOffset(src, pxIdx, 0, ivec2(0, -2)) * -1.0;
	accum += texelFetchOffset(src, pxIdx, 0, ivec2(0, -1)) * -2.0;
	accum += texelFetchOffset(src, pxIdx, 0, ivec2(0,  0)) * +6.0;
	accum += texelFetchOffset(src, pxIdx, 0, ivec2(0, +1)) * -2.0;
	accum += texelFetchOffset(src, pxIdx, 0, ivec2(0, +2)) * -1.0;

	#ifdef MONO
		mediump float mono = dot(accum.rgb, vec3(0.299, 0.587, 0.114)) * accum.a;
		accum = vec4(vec3(mono), 1.0);
	#endif

	#if defined(BINARY)
		accum = step(BINARY, accum);
	#endif

	result = accum;
}
