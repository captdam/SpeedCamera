#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <errno.h>

#include "common.h"
#include "gl.h"

#include "source.h"

#define WINDOW_RATIO 1

#define DEBUG_STAGE 1
#define DEBUG_FILE_DIR "./debugspace/debug.data"

int main(int argc, char* argv[]) {
	int status = EXIT_FAILURE;
	size_t frameCount = 0;
	vh_t videoInfo;
	size2d_t size;

	Source source = NULL;
	GL gl = NULL;

	gl_obj shaderDirectRender = 0;
	gl_mesh meshStdRect = {0, 0, 0, 0};
	gl_obj texture = 0;

	source = source_init(argv[1], &videoInfo);
	if (!source) {
		fputs("Fail to init source input.\n", stderr);
		goto label_exit;
	}
	if (videoInfo.colorScheme != 1 && videoInfo.colorScheme != 3 && videoInfo.colorScheme != 4) {
		fprintf(stderr, "Bad color scheme, support 1, 3 and 4 only, got %"PRIu16".\n", videoInfo.colorScheme);
		goto label_exit;
	}
	size = (size2d_t){.width=videoInfo.width, .height=videoInfo.height};

	gl = gl_init(size, WINDOW_RATIO);
	if (!gl) {
		fputs("Cannot init OpenGL.\n", stderr);
		goto label_exit;
	}

	shaderDirectRender = gl_loadShader("shader/stdRect.vs.glsl", "shader/directRender.fs.glsl");
	if (!shaderDirectRender) {
		fputs("Cannot load program.\n", stderr);
		goto label_exit;
	}

	gl_vertex_t vertices[] = {
		/*tr*/ +1.0f, +1.0f, 1.0f, 0.0f,
		/*br*/ +1.0f, -1.0f, 1.0f, 1.0f,
		/*bl*/ -1.0f, -1.0f, 0.0f, 1.0f,
		/*tl*/ -1.0f, +1.0f, 0.0f, 0.0f
	};
	gl_index_t attributes[] = {4};
	gl_index_t indices[] = {0, 3, 2, 0, 2, 1};
	meshStdRect = gl_createMesh((size2d_t){.height=4, .width=4}, 6, attributes, vertices, indices);

	texture = gl_createTexture(videoInfo, NULL);


	gl_fb defaultFrame = {.frame = 0, .texture = 0};
	double lastFrameTime = 0;
	while(!gl_close(gl, -1)) {
		if (!source_read(source, texture))
			gl_close(gl, 1);
		size2d_t windowSize = gl_drawStart(gl);

		gl_bindFrameBuffer(&defaultFrame, windowSize, 0);

		gl_useShader(&shaderDirectRender);
		gl_bindTexture(&texture, 0);
		gl_drawMesh(&meshStdRect);

		char title[60];
		sprintf(title, "Viewer - frame %zu", frameCount++);
		gl_drawEnd(gl, title);
	}




	status = EXIT_SUCCESS;
label_exit:
	gl_deleteTexture(&texture);
	gl_deleteMesh(&meshStdRect);
	gl_unloadShader(&shaderDirectRender);

	gl_close(gl, 1);
	gl_destroy(gl);
	source_destroy(source);
	fprintf(stdout, "%zu frames displayed.\n\n", frameCount);
	return EXIT_SUCCESS;
}