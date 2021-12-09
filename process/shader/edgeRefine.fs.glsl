in vec2 currentPos;
out vec4 result;

uniform sampler2D src;

void main() {
	ivec2 currentIdx = ivec2( vec2(textureSize(src, 0)) * currentPos );

	float refined = 0.0;
	if (texelFetch(src, currentIdx, 0).r > 0.0 && texelFetchOffset(src, currentIdx, 0, ivec2(0, 1)).r <= 0.0)
		refined = 1.0;

	/** Refined edge
	 * A single float number, use R channel only
	 */
	result = vec4(refined, 0.0, 0.0, 0.0);
}
