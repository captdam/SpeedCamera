/** Layout 0: Screen-domain coord of speedometer
 * format: {{left_x, top_x, right_x, bottom_y}, ...}
 */
layout (location = 0) in vec4 vertices;
#define VERTICES_DRAW vertices.xy
#define VERTICES_DRAW_X vertices.x
#define VERTICES_DRAW_Y vertices.y
#define VERTICES_SAMPLE vertices.zw
#define VERTICES_SAMPLE_X vertices.z
#define VERTICES_SAMPLE_Y vertices.w

out vec2 currentPos;
out flat int speed;

uniform sampler2D speedmap;

void main() {
	//Speedometer screen position
	gl_Position = vec4(VERTICES_DRAW_X * 2.0 - 1.0, VERTICES_DRAW_Y * 2.0 - 1.0, 0.0, 1.0);

	//Find out which corner of the box: coord of the speedometer texture
	currentPos = vec2(
		VERTICES_DRAW_X > VERTICES_SAMPLE_X ? 1.0 : 0.0,
		VERTICES_DRAW_Y > VERTICES_SAMPLE_Y ? 1.0 : 0.0
	);

	//Speed: Which speedometer image to display
	ivec2 sampleIdx = ivec2( vec2(textureSize(speedmap, 0)) * VERTICES_SAMPLE );
	speed = int(texelFetch(speedmap, sampleIdx, 0).r);
}
