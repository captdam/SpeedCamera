#version 310 es

precision highp float;

layout (location = 0) in vec4 position;
layout (location = 1) in vec4 boundary;

uniform vec2 size;
uniform sampler2D pStage;

uniform vec2 glyphSize;
uniform vec4 bias;

/** For bias:
 - xy: result = bias.y * input + bias.x
 - z: Only count pixels with speed higher than this value (m/s)
 - w: Only displce when this much pixels has valid data (%)
*/
#define BIAS_CONSTANT x
#define BIAS_FIRST_ORDER y
#define BIAS_SPEED_SENSITIVITY z
#define BIAS_DISPLAY_SENSITIVITY w

out vec2 textCord;

const int speedPoolSize = 8;

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
	//Speedometer screen position
	gl_Position = vec4(position.xy, 0.0, 1.0);

	//Sample speed
	int searchLeft = int(size.x * boundary.x), searchRight = int(size.x * boundary.y);
	int searchTop = int(size.y * boundary.z), searchBottom = int(size.y * boundary.w);
	int numPixel = (searchRight - searchLeft) * (searchBottom - searchTop);

	float validCount = 0.0;
	float speedPool[speedPoolSize];
	for (int i = 0; i < speedPoolSize; i++)
		speedPool[i] = 0.0;
	
	for (int y = searchTop; y < searchBottom; y++) {
		for (int x = searchLeft; x < searchRight; x++) {
			float currentSpeed = texelFetch(pStage, ivec2(x, y), 0).r;
			if (currentSpeed > bias.BIAS_SPEED_SENSITIVITY) {
				replaceMin(speedPool, currentSpeed);
				validCount += 1.0;
			}
		}
	}

	//Remove noise
	int sIdx = 0;
	if (validCount / float(numPixel) >= bias.BIAS_DISPLAY_SENSITIVITY / 100.0)
		sIdx = int(bias.y * avgExZero(speedPool) + bias.x);
	
	//Get coord in speedometer glyph texture
	vec2 indSize = vec2(glyphSize.x / 16.0, glyphSize.y / 16.0);
	vec2 offset = vec2(float(sIdx % 16) * indSize.x, float(sIdx / 16) * indSize.y);
	textCord = vec2(position.z * indSize.x + offset.x, position.w * indSize.y + offset.y);

}
