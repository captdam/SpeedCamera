#version 310 es

precision mediump float;

in vec2 textpos;

uniform sampler2D bitmap;

uniform unsigned int width;
uniform vec3 maskTop;
uniform vec3 maskMiddle;
uniform vec3 maskBottom;

out vec4 color;

void main() {
	vec4 c = texture(bitmap, textpos);
	float luma = (c.x + c.y + c.z) / 3.0;
	luma = float(width) / 1000.0;
	color = vec4(luma, luma, luma, 1.0);
}
