#version 310 es

precision mediump float;

in vec2 textpos;

uniform sampler2D bitmap;

out vec4 color;

void main() {
	color = texture(bitmap, textpos);
}