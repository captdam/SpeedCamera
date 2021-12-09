in vec2 currentPos;
out vec4 result;

uniform sampler2D src;

const ivec2 idx[] = ivec2[](
	ivec2(-2, -2),	ivec2(-1, -2),	ivec2( 0, -2),	ivec2(+1, -2),	ivec2(+2, -2),
	ivec2(-2, -1),	ivec2(-1, -1),	ivec2( 0, -1),	ivec2(+1, -1),	ivec2(+2, -1),
	ivec2(-2,  0),	ivec2(-1,  0),	ivec2( 0,  0),	ivec2(+1,  0),	ivec2(+2,  0),
	ivec2(-2, +1),	ivec2(-1, +1),	ivec2( 0, +1),	ivec2(+1, +1),	ivec2(+2, +1),
	ivec2(-2, +2),	ivec2(-1, +2),	ivec2( 0, +2),	ivec2(+1, +2),	ivec2(+2, +2)
);
const float mask[] = float[](
	1.0 / 256.0,	4.0 / 256.0,	6.0 / 256.0,	4.0 / 256.0,	1.0 / 256.0,
	4.0 / 256.0,	16.0 / 256.0,	24.0 / 256.0,	16.0 / 256.0,	4.0 / 256.0,
	6.0 / 256.0,	24.0 / 256.0,	36.0 / 256.0,	24.0 / 256.0,	16.0 / 256.0,
	4.0 / 256.0,	16.0 / 256.0,	24.0 / 256.0,	16.0 / 256.0,	4.0 / 256.0,
	1.0 / 256.0,	4.0 / 256.0,	6.0 / 256.0,	4.0 / 256.0,	1.0 / 256.0
);

void main() {
	ivec2 currentIdx = ivec2( vec2(textureSize(src, 0)) * currentPos );

	vec4 accum = vec4(0.0, 0.0, 0.0, 0.0);
	for (int i = 0; i < idx.length(); i++) {
		accum += mask[i] * texelFetchOffset(src, currentIdx, 0, idx[i]);
	}

	result = accum;
}
