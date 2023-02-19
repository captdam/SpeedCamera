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

#define THRESHOLD 0.1
//#define THRESHOLD vec4(0.05, 0.03, 0.05, -0.1) //Use different threshold for different channels

void main() {
	lowp vec3 pb = texture(previous, pxPos).rgb;
	lowp vec3 pc = texture(current, pxPos).rgb;

	lowp vec3 d = abs(pc - pb);
	lowp float diff = dot(d, vec3(0.299, 0.587, 0.114));
	diff = step(THRESHOLD, diff);

	result = diff;
}