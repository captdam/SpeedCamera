layout (location = 0) in highp vec2 position;

out highp vec2 pxPos;

void main() {
	gl_Position = vec4(position.x * 2.0 - 1.0, position.y * 2.0 - 1.0, 0.0, 1.0);
	pxPos = vec2(position.x, 1.0 - position.y); //Fix the difference in screen and GL orientation: y-axis upside-down issue
}