in highp vec2 pxPos;
out lowp vec4 result; //lowp for RGBA8 video

uniform lowp sampler2D current; //lowp for RGBA8 video
uniform lowp sampler2D previous; //lowp for RGBA8 video

/* May defined by client: MONO */
/* May defined by client: BINARY vec4 ^ CLAMP vec4[2]*/

void main() {
	lowp vec4 pb = texture(previous, pxPos);
	lowp vec4 pc = texture(current, pxPos);

	lowp vec4 diff = vec4(0.0);
	#if defined(MONO) //Get luma change
		lowp vec4 d = abs(pc - pb);
		lowp float mono = dot(d, vec4(0.299, 0.587, 0.114, 0.0));
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
