in mediump vec3 gpos;
out lowp vec4 result; //lowp for RGBA8 video

uniform lowp sampler2DArray glyphmap; //lowp for RGBA8 texture

void main() {
	lowp vec4 color = texture(glyphmap, gpos);
	if (color.a == 0.0) discard;
	result = color;
}
