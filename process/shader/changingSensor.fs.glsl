in highp vec2 pxPos;
out lowp float result; //lowp for enum

uniform lowp sampler2D current; //lowp for RGBA8 video
uniform lowp sampler2D previous; //lowp for RGBA8 video

/* May defined by client: BINARY float ^ CLAMP float[2]*/

void main() {
	lowp vec3 pb = texture(previous, pxPos).rgb;
	lowp vec3 pc = texture(current, pxPos).rgb;

	lowp vec3 d = abs(pc - pb);
	lowp float diff = dot(d, vec3(0.299, 0.587, 0.114));

	#if defined(BINARY)
		diff = step(BINARY, diff);
	#elif defined(CLAMP)
		diff = clamp(diff, CLAMP[0], CLAMP[1]);
	#endif

	result = diff;
}
