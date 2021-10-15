#version 310 es

precision highp float;

uniform sampler2D glyphTexture;

in vec2 textCord;
out vec4 nStage;

void main() {
	nStage = texelFetch(glyphTexture, ivec2(textCord), 0);
}
