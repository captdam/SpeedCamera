#version 310 es

precision highp float;

uniform sampler2D pStage;
uniform vec2 size;
in vec2 textpos;
out vec4 nStage;

uniform float threshold;

void main() {
	const vec4 rgb2g = vec4(0.2989, 0.5870, 0.1140, 0.0);
	const ivec2 nIdx[8] = ivec2[8](
		ivec2(-1, -1), ivec2( 0, -1), ivec2(+1, -1),
		ivec2(-1,  0),                ivec2(+1,  0),
		ivec2(-1, +1), ivec2( 0, +1), ivec2(+1, +1)
	);

	float ts = threshold;
	float tn = threshold;
	ivec2 sIdx = ivec2(
		int(textpos.x * size.x),
		int(textpos.y * size.y)
	);

	float result = 0.0;

	if (dot( texelFetch(pStage, sIdx, 0) , rgb2g ) >= ts) {
		int flag = 0;
		for (int i = 0; i < 8; i++) {
			if (dot( texelFetch(pStage, sIdx + nIdx[i], 0) ,rgb2g) >= tn)
				flag++;
		}
		if (flag >= 2)
			result = 1.0;
	}

	nStage = vec4(result, result, result, 1.0);
}
