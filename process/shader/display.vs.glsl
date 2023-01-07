layout (location = 0) in highp vec4 pos; //Local object pos, glphy texture pos
layout (location = 1) in highp vec3 speed; //Road pos, speed

out mediump vec3 gpos; //Glphy pos, mediump is good enough for small texture

void main() {
	gl_Position = vec4(
		(pos.x + speed.x) * 2.0 - 1.0,
		(pos.y + speed.y) * 2.0 - 1.0,
		0.0,
		1.0
	);
	gpos = vec3(pos.zw, speed.z);
}
