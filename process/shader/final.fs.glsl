in vec2 pxPos;
out vec4 result;

uniform sampler2D orginalTexture;
uniform sampler2D processedTexture;

/* Defined by client: MIX_LEVEL float */

void main() {
	vec4 data = texture(processedTexture, pxPos); //Use texture instead of texelFetch because the window size may differ from data size
	vec3 raw = texture(orginalTexture, pxPos).rgb; //Therefore we need to use texture sampling function provided by GPU
	vec3 processed = max(data.rgb, 0.0) * clamp(data.a, 0.0, 1.0);
	result = vec4( mix(processed, raw, MIX_LEVEL) , 1.0 );
}