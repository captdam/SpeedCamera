in vec2 pxPos;
out vec4 result;

uniform sampler2D src;

/* May defined by client: MONO */
/* May defined by client: BINARY vec4 ^ CLAMP vec4[2]*/
/* Declared by client: const struct Mask {float v; ivec2 idx;} mask[] */

void main() {
	ivec2 pxIdx = ivec2(vec2(textureSize(src, 0)) * pxPos);

	vec4 accum = vec4(0.0, 0.0, 0.0, 0.0);
	for (int i = 0; i < mask.length(); i++) {
		Mask thisMask = mask[i];
		accum += thisMask.v * texelFetch(src, pxIdx + thisMask.idx, 0);
	}

	#ifdef MONO
		float mono = dot(accum.rgb, vec3(0.299, 0.587, 0.114)) * accum.a;
		accum = vec4(vec3(mono), 1.0);
	#endif

	#if defined(BINARY)
		accum = step(BINARY, accum);
	#elif defined(CLAMP)
		accum = clamp(accum, CLAMP[0], CLAMP[1]);
	#endif

	result = accum;
}
