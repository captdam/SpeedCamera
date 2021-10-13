#version 310 es

precision mediump float;

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

out vec2 textCord;

const int speedPoolSize = 16;

void replaceMin(inout float pool[speedPoolSize], float value) {
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

void main() {
	//Speedometer screen position
	gl_Position = vec4(position.xy, 0.0, 1.0);

	float speed = 0.0;

	//Sample speed
	float accum = 0.0, count = 0.0;
	int searchLeft = int(size.x * boundary.x), searchRight = int(size.x * boundary.y);
	int searchTop = int(size.y * boundary.z), searchBottom = int(size.y * boundary.w);
	int numPixel = (searchRight - searchLeft) * (searchBottom - searchTop);

	float speedPool[speedPoolSize];
	for (int i = 0; i < speedPoolSize; i++)
		speedPool[i] = 0.0;
	
	for (int y = searchTop; y < searchBottom; y++) {
		for (int x = searchLeft; x < searchRight; x++) {
			float currentSpeed = texelFetch(pStage, ivec2(x, y), 0).r;
			if (currentSpeed > bias.z) {
				replaceMin(speedPool, currentSpeed);
				count += 1.0;
			}
		}
	}

	//Remove noise
	int sIdx = 0;
	if (count / float(numPixel) >= bias.w / 100.0) {

		//Avg but not include 0
		float validCount = 0.0;
		for (int i = 0; i < speedPoolSize; i++) {
			if (speedPool[i] > 0.0) {
				validCount = validCount + 1.0;
				speed = speed + speedPool[i];
			}
		}
		speed = speed / validCount;

		//Apply bias
		float biased = bias.y * speed + bias.x;
		sIdx = int(biased);
	}
	
	//Get coord in speedometer glyph texture
	vec2 indSize = vec2(glyphSize.x / 16.0, glyphSize.y / 16.0);
	vec2 offset = vec2(float(sIdx % 16) * indSize.x, float(sIdx / 16) * indSize.y);
	textCord = vec2(position.z * indSize.x + offset.x, position.w * indSize.y + offset.y);

}
