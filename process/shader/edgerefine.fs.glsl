#version 310 es

/* Texture type = R_8 */

precision mediump float;

uniform sampler2D pStage;
uniform vec2 size;
in vec2 textpos;
out vec4 nStage;

uniform float threshold;

layout (std140) uniform FilterMask {
	vec3 maskTop;
	vec3 maskMiddle;
	vec3 maskBottom;
};

void main() {
	const vec4 rgb2g = vec4(0.2989, 0.5870, 0.1140, 0.0);
	ivec2 texelIndex = ivec2(
		int(textpos.x * size.x),
		int(textpos.y * size.y)
	);
	float nw = dot( texelFetchOffset(pStage, texelIndex, 0, ivec2(-1, -1)) , rgb2g);
	float  n = dot( texelFetchOffset(pStage, texelIndex, 0, ivec2( 0, -1)) , rgb2g);
	float ne = dot( texelFetchOffset(pStage, texelIndex, 0, ivec2(+1, -1)) , rgb2g);
	float  w = dot( texelFetchOffset(pStage, texelIndex, 0, ivec2(-1,  0)) , rgb2g);
	float  x = dot( texelFetchOffset(pStage, texelIndex, 0, ivec2( 0,  0)) , rgb2g);
	float  e = dot( texelFetchOffset(pStage, texelIndex, 0, ivec2(+1,  0)) , rgb2g);
	float sw = dot( texelFetchOffset(pStage, texelIndex, 0, ivec2(-1, +1)) , rgb2g);
	float  s = dot( texelFetchOffset(pStage, texelIndex, 0, ivec2( 0, +1)) , rgb2g);
	float se = dot( texelFetchOffset(pStage, texelIndex, 0, ivec2(+1, +1)) , rgb2g);

	float dnw = x - nw;
	float  dn = x - n;
	float dne = x - ne;
	float  dw = x - w;
	float  de = x - e;
	float dsw = x - sw;
	float  ds = x - s;
	float dse = x - se;

	vec4 result = vec4(0.0, 0.0, 0.0, 0.0);

/*	if (min(dn, ds) > threshold)
		result = 1.0;
	else if (min(dw, de) > threshold)
		result = 1.0;
	else if (min(dnw, dse) > threshold)
		result = 1.0;
	else if (min(dne, dsw) > threshold)
		result = 1.0;*/
	
/*	if (x > threshold && !(min(dn, ds) > threshold && min(dw, de) > threshold) )
		result = 1.0;*/
	
	float tx = threshold;
	float tn = threshold;

	if (x > tx) {
		int flag = 0;
		if (nw > tn) flag++;
		if (n > tn) flag++;
		if (ne > tn) flag++;
		if (w > tn) flag++;
		if (e > tn) flag++;
		if (sw > tn) flag++;
		if (s > tn) flag++;
		if (se > tn) flag++;
		if (flag >= 2) result = vec4(1.0, 1.0, 1.0, 1.0);
	}

	nStage = result;
}