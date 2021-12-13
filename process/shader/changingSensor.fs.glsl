in vec2 currentPos;
out vec4 result;

uniform sampler2D current;
uniform sampler2D previous;

const mat3 rgb2yuv = mat3(
	0.299,	0.587,	0.114,
	-0.147,	-0.289,	0.436,
	0.615,	-0.515,	-0.1
);
const vec4 weight = vec4(
	1.0,	//Luma
	2.0,	//Color-U
	2.0,	//Color-V
	0.0	//Color-RGBavg
);

void main() {
	ivec2 currentIdx = ivec2( vec2(textureSize(current, 0)) * currentPos );

	vec3 currentRGB = texelFetch(current, currentIdx, 0).rgb;
	vec3 currentYUV = currentRGB * rgb2yuv;
	vec3 previousRGB = texelFetch(previous, currentIdx, 0).rgb;
	vec3 previousYUV = previousRGB * rgb2yuv;

	/** Vector scheme: 
	 * 0:	Changing in luma; 
	 * 1,2:	Changing in color UV; 
	 * 3:	Changing in RGB. 
	 */
	vec4 diff = max(vec4(
		currentYUV - previousYUV,
		dot(currentRGB - previousRGB, vec3(0.333, 0.333, 0.333))
	), 0.0);

	/** Weighted changing
	 * A single float number, use R channel only
	 */
	float weighted = dot(diff, weight);
	result = vec4(weighted, 0.0, 0.0, weighted);
}
