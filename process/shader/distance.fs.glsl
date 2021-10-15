#version 310 es

precision highp float;

uniform vec2 size;
in vec2 textpos;
out vec4 nStage;

uniform sampler2D roadmap;
uniform sampler2D ta;
uniform sampler2D tb;
uniform float maxDistance;

#define ROADMAP_POSITION xy
#define ROADMAP_SEARCHREGION zw

void main() {
	const float threshold = 0.5;

	const ivec2 vSearchSelfOffsets[] = ivec2[](
		ivec2(0, 0),
		ivec2(1, 0), ivec2(-1, 0),
		ivec2(2, 0), ivec2(-2, 0),
		ivec2(3, 0), ivec2(-3, 0),
		ivec2(4, 0), ivec2(-4, 0)
	);
	const ivec2 vSearchTargetOffsets[] = ivec2[](
		ivec2(0, 0),
		ivec2(1, 0), ivec2(-1, 0),
		ivec2(2, 0), ivec2(-2, 0),
		ivec2(3, 0), ivec2(-3, 0),
		ivec2(4, 0), ivec2(-4, 0)
	);
	
	ivec2 isize = ivec2(int(size.x), int(size.y));
	ivec2 texelIndex = ivec2(
		int(textpos.x * size.x),
		int(textpos.y * size.y)
	);

	vec4 currentRoadmap = texelFetch(roadmap, texelIndex, 0);
	vec2 currentPos = currentRoadmap.ROADMAP_POSITION;
	ivec2 searchRegion = ivec2(currentRoadmap.ROADMAP_SEARCHREGION);

	float dis = maxDistance; //Displacement of edge

/*	if (texelFetch(ta, texelIndex, 0).r >= threshold) { //Edge is new in current frame
		for (int left = 1; left < searchRegion.x && left < texelIndex.x; left++) { //Do not continue search if out of region. The OpenGL takes care of index fault, but dry-run costs performance
			ivec2 targetIndex = texelIndex + ivec2(-left, 0);
			if (texelFetch(tb, targetIndex, 0).r >= threshold) {
				vec2 targetPos = texelFetch(roadmap, targetIndex, 0).ROADMAP_POSITION;
				dis = min(dis, distance(currentPos, targetPos)); //Replace the value if find closer displacement in this direction
				break;
			}
		}
		for (int right = 1; right < searchRegion.x && right + texelIndex.x < isize.x; right++) {
			ivec2 targetIndex = texelIndex + ivec2(right, 0);
			if (texelFetch(tb, targetIndex, 0).r >= threshold) {
				vec2 targetPos = texelFetch(roadmap, targetIndex, 0).ROADMAP_POSITION;
				dis = min(dis, distance(currentPos, targetPos));
				break;
			}
		}*/
	
	int vSearchSelfCnt = 0;
	for (int i = 0; i < vSearchSelfOffsets.length(); i++) {
		if (texelFetch(ta, texelIndex + vSearchSelfOffsets[i], 0).r >= threshold)
			vSearchSelfCnt++;
	}
	if (vSearchSelfCnt >= vSearchSelfOffsets.length()) {
		for (int up = 1; up < searchRegion.y && up < texelIndex.y; up++) {
			ivec2 targetIndex = texelIndex + ivec2(0, -up);

			int vSearchTargetCnt = 0;
			for (int i = 0; i < vSearchTargetOffsets.length(); i++) {
				if (texelFetch(tb, targetIndex + vSearchTargetOffsets[i], 0).r >= threshold)
					vSearchTargetCnt++;
			}
			if (vSearchTargetCnt > vSearchTargetOffsets.length() / 2) {
				vec2 targetPos = texelFetch(roadmap, targetIndex, 0).ROADMAP_POSITION;
				dis = min(dis, distance(currentPos, targetPos));
				break;
			}
		}
		for (int down = 1; down < searchRegion.y && down + texelIndex.y < isize.y; down++) {
			ivec2 targetIndex = texelIndex + ivec2(0, down);
			
			int vSearchTargetCnt = 0;
			for (int i = 0; i < vSearchTargetOffsets.length(); i++) {
				if (texelFetch(tb, targetIndex + vSearchTargetOffsets[i], 0).r >= threshold)
					vSearchTargetCnt++;
			}
			if (vSearchTargetCnt > vSearchTargetOffsets.length() / 2) {
				vec2 targetPos = texelFetch(roadmap, targetIndex, 0).ROADMAP_POSITION;
				dis = min(dis, distance(currentPos, targetPos));
				break;
			}
		}
	}


	float result = 0.0;
	if (dis != maxDistance) //Nearest edge found, hence the dis has been modified
		result = dis;

	nStage = vec4(result, result, result, 1.0);
}
