#version 310 es

precision mediump float;

in vec2 textpos;

uniform sampler2D bitmap;

uniform uvec2 size;
uniform vec3 maskTop;
uniform vec3 maskMiddle;
uniform vec3 maskBottom;

out vec4 color;

void main() {
	vec4 c = texture(bitmap, textpos);

	vec2 offset = vec2(1.0f/float(size.x), 1.0f/float(size.y));
	vec2 pos[9] = vec2[](
		vec2(-offset.x,-offset.y),	vec2(0.0f,-offset.y),	vec2(+offset.x,-offset.y),
		vec2(-offset.x,0.0f),		vec2(0.0f,0.0f),	vec2(+offset.x,0.0f),
		vec2(-offset.x,+offset.y),	vec2(0.0f,+offset.y),	vec2(+offset.x,+offset.y)
	);

	float mask[9] = float[](
		maskTop.x,	maskTop.y,	maskTop.z,
		maskMiddle.x,	maskMiddle.y,	maskMiddle.z,
		maskBottom.x,	maskBottom.y,	maskBottom.z
	);

	vec4 accum = vec4(0.0f, 0.0f, 0.0f, 0.0f);
	for (int i = 0; i < 9; i++) {
		accum += texture(bitmap, textpos + pos[i]) * mask[i];
	}

	color = vec4(accum.x, accum.y, accum.z, 1.0f);

//	vec4 tl = texture(bitmap, texture - )
//	float luma = (c.x + c.y + c.z) / 3.0;
//	luma = float(width) / 1000.0;
//	color = vec4(luma, luma, luma, 1.0);
}
