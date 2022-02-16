in vec2 pxPos;
out vec4 result;

uniform sampler2D current;
uniform sampler2D previous;

/* May defined by client: MONO ^ HUE */
/* May defined by client: BINARY vec4 ^ CLAMP vec4[2]*/

float hue(vec3 rgb) {
	float max = max(max(rgb.r, rgb.g), rgb.b);
	float min = min(min(rgb.r, rgb.g), rgb.b);

	float hue = 0.0;
	if (max == rgb.r)
		hue = (rgb.g - rgb.b) / (max - min);
	else if (max == rgb.g)
		hue = (rgb.b - rgb.r) / (max - min) + 2.0;
	else
		hue = (rgb.r - rgb.g) / (max - min) + 4.0;

	if (hue >= 6.0)
		hue -= 6.0;
	else if (hue < 0.0)
		hue += 6.0;

	return hue / 6.0;
}

void main() {
	ivec2 pxIdx = ivec2(vec2(textureSize(current, 0)) * pxPos);
	vec4 pb = texelFetch(previous, pxIdx, 0);
	vec4 pc = texelFetch(current, pxIdx, 0);

	vec4 diff = vec4(0.0);
	#if defined(HUE) //Get hue change
		float hp = hue(pb.rgb);
		float hc = hue(pc.rgb);
		diff = vec4(abs(hc - hp));
	#elif defined(MONO) //Get luma change
		vec3 d = abs(pc.rgb - pb.rgb);
		float mono = dot(d, vec3(0.299, 0.587, 0.114));
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
