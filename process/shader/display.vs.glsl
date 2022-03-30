/** Layout 0: Screen-domain coord of focus region mesh in range [0,1]
 * format: {{display_x, display_y, speed, oritiation}, ...}
 *                                        |||||||||| 0.0(invalid), -1.0(top-left), -2.0(left-bottom), +1.0(right-top), +2.0(right-bottom)
 */
layout (location = 0) in highp vec4 vertices;

/** Output: Current render pixel position
 */
out mediump vec2 pos; //Glphy pos, mediump is good enough for small texture
flat out lowp uint speed; //max 255

void main() {
	gl_Position = vec4(vertices.x * 2.0 - 1.0, vertices.y * 2.0 - 1.0, 0.0, 1.0);
	if (vertices.w == -1.0)
		pos = vec2(0.0, 0.0);
	else if (vertices.w == -2.0)
		pos = vec2(0.0, 1.0);
	else if (vertices.w == +1.0)
		pos = vec2(1.0, 0.0);
	else if (vertices.w == +2.0)
		pos = vec2(1.0, 1.0);
	else
		pos = vec2(0.5);
	speed = uint(vertices.z);
}
