#version 310 es

precision mediump float;

//Layout: {{vertex_x, vertex_y, texture_x, texture_y}, ...}
layout (location = 0) in vec2 position;

out vec2 textpos;

void main() {
	gl_Position = vec4(position.x * 2.0f - 1.0f, position.y * 2.0f - 1.0f, 0.0f, 1.0f);
	textpos = position.xy;
}
