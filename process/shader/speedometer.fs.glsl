in vec2 currentPos;
in flat int speed;

out vec4 dest;

uniform mediump sampler2DArray glyphmap;

void main() {
	dest = texelFetch(glyphmap, ivec3(vec2(textureSize(glyphmap, 0)) * currentPos.xy, speed), 0);
//	dest = vec4(currentPos.xy, 0.5, 1.0);
}
