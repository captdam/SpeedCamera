in vec2 currentPos;
out vec4 result;

uniform sampler2D src;

const int idx[] = int[](	-2,	-1,	0,	1,	2	);
const float mask[] = float[](	-4.0,	-1.0,	0.0,	+1.0,	+4.0	);

const float resultDenoiseValue = 0.5;

void main() {
	ivec2 currentIdx = ivec2( vec2(textureSize(src, 0)) * currentPos );

	float accum = 0.0;
	for (int i = 0; i < idx.length(); i++) {
		accum += mask[i] * texelFetchOffset(src, currentIdx, 0, ivec2(0, idx[i])).r;
	}

	accum = accum - resultDenoiseValue;

	/** Denoised edge
	 * A single float number, use R channel only
	 */
	result = vec4(accum, 0.0, 0.0, 0.0);
}
