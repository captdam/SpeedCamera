#version 310 es

precision mediump float;

layout (location = 0) in vec4 position;
layout (location = 1) in vec4 boundary;

uniform vec2 size;
uniform sampler2D pStage;

uniform vec2 glyphSize;
uniform vec4 bias; //3: speed sensitivity, 2-0: x^n
uniform float sensitivity; //Filter sensitivity

out vec2 textCord;

void main() {
	//Speedometer screen position
	gl_Position = vec4(position.xy, 0.0, 1.0);

	//Sample speed
	float accum = 0.0, count = 0.0;
	int searchLeft = int(size.x * boundary.x), searchRight = int(size.x * boundary.y);
	int searchTop = int(size.y * boundary.z), searchBottom = int(size.y * boundary.w);
	int numPixel = (searchRight - searchLeft) * (searchBottom - searchTop);
	for (int y = searchTop; y < searchBottom; y++) {
		for (int x = searchLeft; x < searchRight; x++) {
			float speed = texelFetch(pStage, ivec2(x, y), 0).r;
			if (speed > bias.w) {
				accum += speed;
				count += 1.0;
			}
		}
	}

	//Bias and remove noise
	int netSpeed = 0;
	if (count / float(numPixel) >= sensitivity) {
		float unbiased = accum / count;
		float biased = bias.z * unbiased * unbiased + bias.y * unbiased + bias.x;
		netSpeed = int(biased);
	}
	
	//Get coord in speedometer glyph texture
	vec2 indSize = vec2(glyphSize.x / 16.0, glyphSize.y / 16.0);
	vec2 offset = vec2(float(netSpeed % 16) * indSize.x, float(netSpeed / 16) * indSize.y);
	textCord = vec2(position.z * indSize.x + offset.x, position.w * indSize.y + offset.y);

}
