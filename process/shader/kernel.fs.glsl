in highp vec2 pxPos;
out lowp vec4 result; //lowp for RGBA8 video

uniform lowp sampler2D src; //lowp for RGBA8 video

/* May defined by client: MONO */
/* May defined by client: BINARY vec4 ^ CLAMP vec4[2]*/
/* Declared by client: const struct Mask {float v; ivec2 idx;} mask[] */

void main() {
	mediump ivec2 srcSize = textureSize(src, 0);
	mediump vec2 srcSizeF = vec2(srcSize);

	mediump ivec2 pxIdx = ivec2( srcSizeF * pxPos );

	mediump vec4 accum = vec4(0.0, 0.0, 0.0, 0.0); //During accum, may excess lowp range
	for (int i = 0; i < mask.length(); i++) {
		Mask thisMask = mask[i];
		mediump ivec2 idxOffset = thisMask.idx;
		mediump float weight = thisMask.v;
		accum += weight * texelFetch(src, pxIdx + idxOffset, 0);
	}

	#ifdef MONO
		mediump float mono = dot(accum.rgb, vec3(0.299, 0.587, 0.114)) * accum.a;
		accum = vec4(vec3(mono), 1.0);
	#endif

	#if defined(BINARY)
		accum = step(BINARY, accum);
	#elif defined(CLAMP)
		accum = clamp(accum, CLAMP[0], CLAMP[1]);
	#endif

	result = accum;
}
