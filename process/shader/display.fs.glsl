in mediump vec2 pos;
flat in lowp uint speed;
out lowp vec4 result; //lowp for RGBA8 video

uniform lowp sampler2DArray glyphmap; //lowp for RGBA8 texture

/* Defined by client: SIZE ivec2 */

void main() {
	result = texture(glyphmap, vec3(pos, speed));
}
