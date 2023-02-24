@VS

layout (location = 0) in highp vec2 meshROI;
out mediump vec2 pxPos;

void main() {
	gl_Position = vec4(meshROI.x * 2.0 - 1.0, meshROI.y * 2.0 - 1.0, 0.0, 1.0);
	pxPos = meshROI.xy;
}

@FS

uniform mediump sampler2D src; //mediump for general data
uniform mediump sampler2DArray roadmap;
uniform lowp int mode; //RGBA index: 3=P2O 4=O2P

in mediump vec2 pxPos;
out mediump vec4 result; //mediump for general data

void main() {
	//Road surface is linear, so does the projection table, ok to use texture filter
	//Data may not be linear, so do not use filter

	mediump ivec2 srcSize = textureSize(src, 0);
	mediump vec2 srcSizeF = vec2(srcSize);

	mediump ivec2 projIdx = ivec2( srcSizeF * pxPos );
	projIdx.x = int( srcSizeF.x * texture(roadmap, vec3(pxPos, 1.0))[mode] );
	result = texelFetch(src, projIdx, 0);
}