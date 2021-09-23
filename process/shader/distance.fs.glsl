#version 310 es

precision mediump float;
precision mediump int;

uniform vec2 size;
in vec2 textpos;
out vec4 nStage;

uniform sampler2D roadmap;
uniform sampler2D ta;
uniform sampler2D tb;
uniform float maxDistance;

void main() {
	const float et = 0.5; //Edge threshold. mat=0, exp=-1, Use >= and < so the hardware only looks the exp
	
	ivec2 isize = ivec2(int(size.x), int(size.y));
	ivec2 texelIndex = ivec2(
		int(textpos.x * size.x),
		int(textpos.y * size.y)
	);

	vec4 currentRoadmap = texelFetch(roadmap, texelIndex, 0);
	vec2 currentPos = currentRoadmap.xy;
	ivec2 searchRegion = ivec2(currentRoadmap.zw);

	float dis = maxDistance; //Displacement of edge

	vec4 result = vec4(0.0, 0.0, 0.0, 0.0);;

	if (texelFetch(ta, texelIndex, 0).r >= et) { //Edge is new in current frame
		for (int left = 1; left < searchRegion.x && left < texelIndex.x; left++) { //Do not continue search if out of region. The OpenGL takes care of index fault, but dry-run costs performance
			ivec2 targetIndex = texelIndex + ivec2(-left, 0);
			if (texelFetch(tb, targetIndex, 0).r >= et) {
				vec2 targetPos = texelFetch(roadmap, targetIndex, 0).xy;
				dis = min(dis, distance(currentPos, targetPos));
				break;
			}
		}
		for (int right = 1; right < searchRegion.x && right + texelIndex.x < isize.x; right++) {
			ivec2 targetIndex = texelIndex + ivec2(right, 0);
			if (texelFetch(tb, targetIndex, 0).r >= et) {
				vec2 targetPos = texelFetch(roadmap, targetIndex, 0).xy;
				dis = min(dis, distance(currentPos, targetPos));
				break;
			}
		}
		for (int up = 1; up < searchRegion.y && up < texelIndex.y; up++) {
			ivec2 targetIndex = texelIndex + ivec2(0, -up);
			if (texelFetch(tb, targetIndex, 0).r >= et) {
				vec2 targetPos = texelFetch(roadmap, targetIndex, 0).xy;
				dis = min(dis, distance(currentPos, targetPos));
				break;
			}
		}
		for (int down = 1; down < searchRegion.y && down + texelIndex.y < isize.y; down++) {
			ivec2 targetIndex = texelIndex + ivec2(0, down);
			if (texelFetch(tb, targetIndex, 0).r >= et) {
				vec2 targetPos = texelFetch(roadmap, targetIndex, 0).xy;
				dis = min(dis, distance(currentPos, targetPos));
				break;
			}
		}
		
		if (dis != maxDistance) { //Nearest edge found in previous frame, hence the dis has been modified
			dis = dis / maxDistance; //Normalize to [0,1]
			result = vec4(dis, dis, dis, dis);
		}
	}

	nStage = result;
}
