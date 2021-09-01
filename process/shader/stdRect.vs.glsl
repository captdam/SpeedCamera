#version 310 es

precision mediump float;

//Layout: {{vertex_x, vertex_y}, ...}
layout (location = 0) in vec2 position; //Screen-domain

out vec2 textpos;

void main() {
	gl_Position = vec4(position.x * 2.0 - 1.0, position.y * 2.0 - 1.0, 0.0, 1.0);
	textpos = position.xy;
}
