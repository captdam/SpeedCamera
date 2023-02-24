@VS

layout (location = 0) in highp vec2 position;

out mediump vec2 pxPos;

void main() {
	gl_Position = vec4(position.x * 2.0 - 1.0, position.y * 2.0 - 1.0, 0.0, 1.0);
	pxPos = vec2(position.x, 1.0 - position.y); //Fix the difference in screen and GL orientation: y-axis upside-down issue
}

@FS

uniform lowp sampler2D orginalTexture; //lowp for RGBA8 video
uniform lowp sampler2D processedTexture;

in mediump vec2 pxPos;
out lowp vec4 result; //lowp for RGBA8 video

#define RAW_WEIGHT 0.8
#define PROCESSED_WEIGHT 0.9 //data.a

void main() {
	lowp vec4 data = texture(processedTexture, pxPos); //Use texture instead of texelFetch because the window size may differ from data size
	lowp vec4 raw = texture(orginalTexture, pxPos); //Therefore we need to use texture sampling function provided by GPU
	result = RAW_WEIGHT * raw + PROCESSED_WEIGHT * data;
}