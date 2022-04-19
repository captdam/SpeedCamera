/** Layout 0: Screen-domain coord of glyph in NTC
 * format: {{display_x, display_y, speed, not_used_for_align}, ...}
 * Must use direct draw, order is: top-left, bottom-left, top-right, bottom-left, bottom-right, top-right
 */
layout (location = 0) in highp vec4 vertices;

/** Output: Current render pixel position
 */
out mediump vec2 pos; //Glphy pos, mediump is good enough for small texture
flat out lowp uint speed; //max 255

void main() {
	gl_Position = vec4(vertices.x * 2.0 - 1.0, vertices.y * 2.0 - 1.0, 0.0, 1.0);
	if (gl_VertexID % 6 == 0)
		pos = vec2(0.0, 0.0);
	else if (gl_VertexID % 6 == 1)
		pos = vec2(0.0, 1.0);
	else if (gl_VertexID % 6 == 2)
		pos = vec2(1.0, 0.0);
	else if (gl_VertexID % 6 == 3)
		pos = vec2(0.0, 1.0);
	else if (gl_VertexID % 6 == 4)
		pos = vec2(1.0, 1.0);
	else
		pos = vec2(1.0, 0.0);
	speed = uint(vertices.z);
}
