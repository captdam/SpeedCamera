in highp vec2 pxPos;
out lowp vec4 result; //lowp for RGBA8 video

uniform highp sampler2D speedmap;
uniform lowp sampler2DArray glyphmap;

/* Defined by client: SIZE ivec2 */

void main() {
	mediump ivec2 pxIdx = ivec2(vec2(textureSize(speedmap, 0)) * pxPos);
	mediump ivec2 gSize = textureSize(glyphmap, 0).xy;

	mediump vec4 color = vec4(0.0); //Accum, may excess lowp
	mediump float count = 0.0;

	for (mediump int y = pxIdx.y - gSize.y; y < pxIdx.y; y++) {
		for (mediump int x = pxIdx.x - gSize.x; x < pxIdx.x; x++) {
			lowp uint speed = uint(texelFetch(speedmap, ivec2(x,y), 0)); //255 is max
			if (speed != 0U) {
				mediump ivec2 gOffset = ivec2(pxIdx.x - x, pxIdx.y - y);
				color += texelFetch(glyphmap, ivec3(gOffset, speed), 0);
				count += 1.0;
			}
		}
	}

	result = count > 0.0 ? color / vec4(count) : vec4(0.0);
}
