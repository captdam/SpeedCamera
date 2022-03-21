in vec2 pxPos;
out vec4 result;

uniform sampler2D orginalTexture;
uniform sampler2D processedTexture;

/* Defined by client: RAW_LUMA float */

void main() {
	vec4 data = texture(processedTexture, pxPos); //Use texture instead of texelFetch because the window size may differ from data size
	vec4 raw = texture(orginalTexture, pxPos); //Therefore we need to use texture sampling function provided by GPU
	result = mix(data, raw, RAW_LUMA);
}