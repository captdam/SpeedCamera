@VS

layout (location = 0) in highp vec2 meshROI;
out mediump vec2 pxPos;

void main() {
	gl_Position = vec4(meshROI.x * 2.0 - 1.0, meshROI.y * 2.0 - 1.0, 0.0, 1.0);
	pxPos = meshROI.xy;
}

@FS

#define HUMAN

uniform lowp sampler2D src; //lowp for enum
uniform mediump sampler2DArray roadmap;

in mediump vec2 pxPos;
#ifndef HUMAN 
out lowp float result; //lowp for enum
#else
out lowp vec2 result;
#endif

#define SHADER_EDGEREFINE_BOTTOMDENOISE 0.2
#define SHADER_EDGEREFINE_SIDEMARGIN 0.6

#define RESULT_NOTOBJ 0.0
#define RESULT_OBJECT 0.3
#define RESULT_BOTTOM 0.6
#define RESULT_CBEDGE 1.0

// Return true if found valid pixel in limit
bool searchBottom(lowp sampler2D map, mediump ivec2 idx, mediump int limit) {
	for (mediump ivec2 i = idx; i.y < limit; i.y++) {
		if (texelFetch(map, i, 0).r > 0.0)
			return true;
	}
	return false;
}

bvec2 minPath(lowp sampler2D map, mediump ivec2 start, lowp float threshold, mediump int goal) {
	mediump ivec2 lPtr = start, rPtr = start;

	while (start.x - lPtr.x < goal) {
		mediump ivec2 middle = lPtr + ivec2(-1, 0);
		mediump ivec2 up = lPtr + ivec2(-1, -1);
		mediump ivec2 down = lPtr + ivec2(-1, +1);
		if (texelFetch(map, middle, 0).r > threshold)
			lPtr = middle;
		else if (texelFetch(map, up, 0).r > threshold)
			lPtr = up;
		else if (texelFetch(map, down, 0).r > threshold)
			lPtr = down;
		else
			break;
	}

	while (rPtr.x - start.x < goal) {
		mediump ivec2 middle = rPtr + ivec2(+1, 0);
		mediump ivec2 up = rPtr + ivec2(+1, -1);
		mediump ivec2 down = rPtr + ivec2(+1, +1);
		if (texelFetch(map, middle, 0).r > threshold)
			rPtr = middle;
		else if (texelFetch(map, up, 0).r > threshold)
			rPtr = up;
		else if (texelFetch(map, down, 0).r > threshold)
			rPtr = down;
		else
			break;
	}

	return bvec2(start.x - lPtr.x >= goal, rPtr.x - start.x >= goal);
}

lowp float refine() {
	mediump ivec2 srcSize = textureSize(src, 0);
	mediump vec2 srcSizeF = vec2(srcSize);
	mediump ivec2 pxIdx = ivec2(srcSizeF * pxPos);

	//Not object
	if (texelFetch(src, pxIdx, 0).r < 0.1)
		return RESULT_NOTOBJ;

	mediump float pixelWidth = texture(roadmap, vec3(pxPos, 0.0)).w;
	mediump int limitSide = int(SHADER_EDGEREFINE_SIDEMARGIN * pixelWidth);
	mediump int limitBottom = int(SHADER_EDGEREFINE_BOTTOMDENOISE * pixelWidth);

	//Bottom clearence fail, so this is not bottom edge
	if (searchBottom( src , pxIdx + ivec2(0,1) , limitBottom + pxIdx.y + 1 )) //Atleast search 1, do not include self
		return RESULT_OBJECT;

	//If any side has space (edge not at center pertion), then this is not center portion
	if (!all( minPath(src, pxIdx, 0.5, limitSide) ))
		return RESULT_BOTTOM;
	
	//Centered bottom edge
	return RESULT_CBEDGE;
}

void main() {
	#ifndef HUMAN
		result = refine();
	#else
		lowp float x = refine();
		if (x >= RESULT_CBEDGE)
			result = vec2(RESULT_CBEDGE, 1.0);
		else if (x >= RESULT_BOTTOM)
			result = vec2(RESULT_BOTTOM, 1.0);
		else if (x >= RESULT_OBJECT)
			result = vec2(RESULT_OBJECT, 0.1);
		else
			result = vec2(0.0);
	#endif
}