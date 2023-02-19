@VS

layout (location = 0) in highp vec4 pos; //Local object pos, glphy texture pos
layout (location = 1) in highp vec3 speed; //Road pos, speed

out mediump vec3 gpos; //Glphy pos, mediump is good enough for small 2D 256 layers texture

void main() {
	gl_Position = vec4(
		(pos.x + speed.x) * 2.0 - 1.0,
		(pos.y + speed.y) * 2.0 - 1.0,
		0.0,
		1.0
	);
	gpos = vec3(pos.zw, speed.z);
}

@FS

uniform lowp sampler2DArray glyphmap; //lowp for RGBA8 texture

in mediump vec3 gpos;
out lowp vec4 result; //lowp for RGBA8 video

void main() {
	lowp vec4 color = texture(glyphmap, gpos);
	if (color.a == 0.0) discard;
	result = color;
}
