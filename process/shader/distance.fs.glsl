#version 310 es

precision mediump float;
precision mediump int;

uniform vec2 size;
in vec2 textpos;
out vec4 nStage;

uniform sampler2D roadmap;
uniform sampler2D ta;
uniform sampler2D tb;
uniform float threshold;

void main() {
	const float et = 0.5; //Edge threshold. mat=0, exp=-1, Use >= and < so the hardware only looks the exp
	
	ivec2 isize = ivec2(int(size.x), int(size.y));
	ivec2 texelIndex = ivec2(
		int(textpos.x * size.x),
		int(textpos.y * size.y)
	);

	float currentEdge = texelFetch(ta, texelIndex, 0).r;
	float previousEdge = texelFetch(tb, texelIndex, 0).r;
	vec2 currentPos = texelFetch(roadmap, texelIndex, 0).xy;
	ivec2 searchRegion = ivec2(texelFetch(roadmap, texelIndex, 0).zw);

	float dis = threshold; //Displacement of edge

	vec4 result = vec4(0.0, 0.0, 0.0, 0.0);;

	if (currentEdge >= et && previousEdge < et) { //Edge is new in current frame
/*		for (int left = 1; left < searchRegion.x && left < texelIndex.x; left++) { //Do not continue search if out of region. The OpenGL takes care of index fault, but dry-run costs performance
			ivec2 targetIndex = texelIndex + ivec2(-left, 0);
			if (texelFetch(tb, targetIndex, 0).r >= et) {
				vec2 targetPos = texelFetch(roadmap, targetIndex, 0).xy;
				dis = min(dis, distance(currentPos, targetPos));
			}
		}
		for (int right = 1; right < searchRegion.x && right + texelIndex.x < isize.x; right++) {
			ivec2 targetIndex = texelIndex + ivec2(right, 0);
			if (texelFetch(tb, targetIndex, 0).r >= et) {
				vec2 targetPos = texelFetch(roadmap, targetIndex, 0).xy;
				dis = min(dis, distance(currentPos, targetPos));
			}
		}
		for (int up = 1; up < searchRegion.y && up < texelIndex.y; up++) {
			ivec2 targetIndex = texelIndex + ivec2(0, -up);
			if (texelFetch(tb, targetIndex, 0).r >= et) {
				vec2 targetPos = texelFetch(roadmap, targetIndex, 0).xy;
				dis = min(dis, distance(currentPos, targetPos));
			}
		}
		for (int down = 1; down < searchRegion.y && down + texelIndex.y < isize.y; down++) {
			ivec2 targetIndex = texelIndex + ivec2(0, down);
			if (texelFetch(tb, targetIndex, 0).r >= et) {
				vec2 targetPos = texelFetch(roadmap, targetIndex, 0).xy;
				dis = min(dis, distance(currentPos, targetPos));
			}
		}
		
		if (dis != threshold) { //Nearest edge found in previous frame, hence the dis has been modified
			dis = dis / threshold; //Normalize to [0,1]
			result = vec4(1.0, 1.0, 1.0, 1.0);
		}*/
		result = vec4(1.0, 1.0, 1.0, 1.0);
	}

	nStage = result;
}
