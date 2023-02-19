@VS

layout (location = 0) in highp vec2 meshROI;
out mediump vec2 pxPos;

void main() {
	gl_Position = vec4(meshROI.x * 2.0 - 1.0, meshROI.y * 2.0 - 1.0, 0.0, 1.0);
	pxPos = meshROI.xy;
}

@FS

uniform lowp sampler2D src; //lowp for enum, 1 ch
uniform mediump sampler2DArray roadmap; //mediump for object size
uniform lowp vec2 direction; //(1,0) for horizontal and (0,1) for vertical

in mediump vec2 pxPos;
out lowp float result; //lowp for enum

#define SEARCH_DISTANCE 0.7 //Should be object size / 2, or even less

bool search(mediump vec2 center, mediump vec2 step, mediump int cnt, lowp sampler2D img) {
	bvec2 found = bvec2(false);
	mediump vec2 c1 = center, c2 = center;
	for (mediump int i = cnt; i != 0; i--) {
		c1 += step;
		if ( dot(textureGather(img, c1, 0), vec4(1.0)) > 1.0) {
			found.x = true;
			break;
		}
	}
	for (mediump int i = cnt; i != 0; i--) {
		c2 -= step;
		if ( dot(textureGather(img, c2, 0), vec4(1.0)) > 1.0) {
			found.y = true;
			break;
		}
	}
	return found.x && found.y;
}

void main() {
	lowp float ret = texture(src, pxPos).r;

	if (ret < 0.5) {
		mediump ivec2 srcSize = textureSize(src, 0);
		mediump vec2 srcSizeF = vec2(srcSize);

		mediump float pixelWidth = texture(roadmap, vec3(pxPos, 0.0)).w;
		mediump int searchDistancePx = int( 0.5 * SEARCH_DISTANCE * pixelWidth ); //2px a time, so half the count

		mediump vec2 step = vec2(2.0) / srcSizeF; //2px a time for textureGather()
		ret = search(pxPos, direction * step, searchDistancePx, src) ? 0.7 : 0.0;
	}

	result = ret;
}
