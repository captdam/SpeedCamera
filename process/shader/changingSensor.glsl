@VS

layout (location = 0) in highp vec2 meshROI;
out mediump vec2 pxPos;

void main() {
	gl_Position = vec4(meshROI.x * 2.0 - 1.0, meshROI.y * 2.0 - 1.0, 0.0, 1.0);
	pxPos = meshROI.xy;
}

@FS

uniform lowp sampler2D current; //lowp for RGBA8 video
uniform lowp sampler2D previous; //lowp for RGBA8 video

in mediump vec2 pxPos;
out lowp float result; //lowp for enum

#define THRESHOLD 0.2
//#define THRESHOLD vec4(0.05, 0.03, 0.05, -0.1) //Use different threshold for different channels

//lowp vec3 rgb2hsv(lowp vec3 c) {
//	mediump vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
//	mediump vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
//	mediump vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));

//	mediump float d = q.x - min(q.w, q.y);
//	mediump float e = 1.0e-5;
//	return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);

//	/* License: WTFPL */
//}

void main() {
	lowp vec3 pb = texture(previous, pxPos).rgb;
	lowp vec3 pc = texture(current, pxPos).rgb;

	lowp vec3 d = abs(pc - pb);
	lowp float diff = dot(d, vec3(0.299, 0.587, 0.114));
	diff = step(THRESHOLD, diff);
	result = diff;

	/*lowp vec2 b = rgb2hsv(pb).xy; //Get hue and saturation
	lowp vec2 c = rgb2hsv(pc).xy;
	lowp float before = b.x * b.y;
	lowp float current = c.x * c.y;
	if (gl_FragCoord.x == 0.0)
		result = abs(before - current);
	else
		result = c.y;
	//result = step(THRESHOLD, abs(before - current));*/

}