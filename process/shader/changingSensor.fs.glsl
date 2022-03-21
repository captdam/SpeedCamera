in vec2 pxPos;
out vec4 result;

uniform sampler2D current;
uniform sampler2D previous;

/* May defined by client: MONO ^ HUE */
/* May defined by client: BINARY vec4 ^ CLAMP vec4[2]*/

void main() {
	ivec2 pxIdx = ivec2(vec2(textureSize(current, 0)) * pxPos);
	vec4 pb = texelFetch(previous, pxIdx, 0);
	vec4 pc = texelFetch(current, pxIdx, 0);

	vec4 diff = vec4(0.0);
	#if defined(MONO) //Get luma change
		vec4 d = abs(pc - pb);
		float mono = dot(d, vec4(0.299, 0.587, 0.114, 0.0));
		diff = vec4(mono);
	#else //Get raw rgb change
		diff = abs(pc - pb);
	#endif

	#if defined(BINARY)
		diff = step(BINARY, diff);
	#elif defined(CLAMP)
		diff = clamp(diff, CLAMP[0], CLAMP[1]);
	#endif

	result = diff;
}
