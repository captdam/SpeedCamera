/** Layout 0: Screen-domain coord of focus region mesh in range [0,1]
 * format: {{vertex_x, vertex_y}, ...}
 */
layout (location = 0) in highp vec2 meshFocusRegion;

/** Output: Current render pixel position
 */
out highp vec2 pxPos;

void main() {
	gl_Position = vec4(meshFocusRegion.x * 2.0 - 1.0, meshFocusRegion.y * 2.0 - 1.0, 0.0, 1.0);
	pxPos = meshFocusRegion.xy;
}
