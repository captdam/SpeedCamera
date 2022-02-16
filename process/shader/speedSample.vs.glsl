/** Layout 0: Screen-domain coord of speedometer
 * format: {{left_x, top_y, right_x, bottom_y}, ...}
 */
layout (location = 0) in vec4 vertices;
#define VERTICE_POINT1 vertices.xy
#define VERTICE_LEFT vertices.x
#define VERTICE_TOP vertices.y
#define VERTICE_POINT2 vertices.zw
#define VERTICE_RIGHT vertices.z
#define VERTICE_BOTTOM vertices.w

/** Output: Sampled speed
 */
out float speed;

uniform sampler2D speedmap;
#define CH_SPEEDMAP_SPEED r

/** Defined by client: 
const int speedPoolSize
const float speedSensitive
const int speedDisplayCutoff
*/

void replaceMin(inout float pool[speedPoolSize], in float value) {
	int idx = 0;
	for (int i = 0; i < speedPoolSize; i++) {
		if (pool[i] <= 0.0) {
			pool[i] = value;
			return;
		}
		if (pool[i] < pool[idx])
			idx = i;
	}
	pool[idx] = value;
}

float avgExZero(in float pool[speedPoolSize]) {
	float count = 0.0, accum = 0.0;
	for (int i = 0; i < speedPoolSize; i++) {
		if (pool[i] > 0.0) {
			count += 1.0;
			accum += pool[i];
		}
	}
	return accum > 0.0 ? accum / count : 0.0;
}

void main() {
	gl_Position = vec4(VERTICE_LEFT + VERTICE_RIGHT - 1.0, VERTICE_TOP + VERTICE_BOTTOM - 1.0, 0.0, 1.0);

	float s = 0.0;

	float speedPool[speedPoolSize];
	for (int i = 0; i < speedPoolSize; i++)
		speedPool[i] = 0.0;

	ivec2 p1Idx = ivec2( vec2(textureSize(speedmap, 0)) * VERTICE_POINT1 );
	ivec2 p2Idx = ivec2( vec2(textureSize(speedmap, 0)) * VERTICE_POINT2 );

	int validCnt = 0;
	for (int y = p1Idx.y; y < p2Idx.y; y++) {
		for (int x = p1Idx.x; x < p2Idx.x; x++) {
			ivec2 currentIdx = ivec2(x, y);
			float currentSpeed = texelFetch(speedmap, currentIdx, 0).CH_SPEEDMAP_SPEED;
			if (currentSpeed > speedSensitive) {
				replaceMin(speedPool, currentSpeed);
				validCnt++;
			}
		}
	}

	if (validCnt > speedDisplayCutoff) {
		s = avgExZero(speedPool);
	}

	speed = s;
}
