#version 310 es

//Layout: {{vertex_x, vertex_y, texture_x, texture_y}, ...}
layout (location = 0) in vec4 position;

out vec2 textpos;

void main() {
	gl_Position = vec4(position.x, position.y, 0.0f, 1.0f);
	textpos = vec2(position.z, position.w);
}
