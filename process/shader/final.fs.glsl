in highp vec2 pxPos;
out lowp vec4 result; //lowp for RGBA8 video

uniform lowp sampler2D orginalTexture; //lowp for RGBA8 video
uniform lowp sampler2D processedTexture;

/* Defined by client: RAW_LUMA float */

void main() {
	lowp vec4 data = texture(processedTexture, pxPos); //Use texture instead of texelFetch because the window size may differ from data size
	lowp vec4 raw = texture(orginalTexture, pxPos); //Therefore we need to use texture sampling function provided by GPU
	result = vec4(RAW_LUMA) * raw + /*vec4(data.a) * */data;
}