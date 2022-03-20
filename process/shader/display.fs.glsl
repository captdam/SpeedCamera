in vec2 pxPos;
in flat int speed;

out vec4 result;

uniform sampler2D speedmap;
uniform mediump sampler2DArray glyphmap;

/* Defined by client: SIZE ivec2 */

void main() {
	ivec2 pxIdx = ivec2(vec2(textureSize(speedmap, 0)) * pxPos);

	ivec2 gSize = textureSize(glyphmap, 0).xy;

	vec4 color = vec4(0.0);
	int count = 0;

	for (int y = pxIdx.y - gSize.y; y < pxIdx.y; y++) {
		for (int x = pxIdx.x - gSize.x; x < pxIdx.x; x++) {
			int speed = int(texelFetch(speedmap, ivec2(x,y), 0));
			if (speed > 0) {
				ivec2 gOffset = ivec2(pxIdx.x - x, pxIdx.y - y);
				color += texelFetch(glyphmap, ivec3(gOffset, speed), 0);
				count++;
			}
		}
	}

	result = count > 0 ? color / float(count) : vec4(0.0);
}
