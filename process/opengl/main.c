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
	gl_obj texture_orginalFrame = 0;

	gl_fb framebuffer_stage1 = {0, 0};

	gl_obj shader_filter3 = 0;

	gl_mesh mesh_StdRect = {0, 0, 0, 0};

	/* Load orginal frame from file */
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

	/* Init OpenGL and viewer window */
	gl = gl_init(size, WINDOW_RATIO);
	if (!gl) {
		fputs("Cannot init OpenGL.\n", stderr);
		goto label_exit;
	}

	/* Use a texture to store raw frame data */
	texture_orginalFrame = gl_createTexture(videoInfo, NULL);

	/* Drawing on frame buffer to process data */
	framebuffer_stage1 = gl_createFrameBuffer(videoInfo);

	/* Process - Kernel filter 3x3 */
	const char* shader_filter3_paramName[] = {"width", "maskTop", "maskMiddle", "maskBottom"};
	gl_param shader_filter3_paramId[4];
	shader_filter3 = gl_loadShader("shader/stdRect.vs.glsl", "shader/filter3.fs.glsl", shader_filter3_paramName, shader_filter3_paramId, 4);
	if (!shader_filter3) {
		fputs("Cannot load program.\n", stderr);
		goto label_exit;
	}
	gl_param shader_filter3_paramWidth = shader_filter3_paramId[0];
	gl_param shader_filter3_paramMaskTop = shader_filter3_paramId[1];
	gl_param shader_filter3_paramMaskMiddle = shader_filter3_paramId[2];
	gl_param shader_filter3_paramMaskBottom = shader_filter3_paramId[3];
	gl_useShader(&shader_filter3);
	unsigned int width = size.width; //Cast to unsigned int first, size is of size_t
	gl_setShaderParam(shader_filter3_paramWidth, 1, gl_type_uint, &width);


	/* Drawing mash (simple rect) */
	gl_vertex_t vertices[] = {
		/*tr*/ +1.0f, +1.0f, 1.0f, 1.0f,
		/*br*/ +1.0f, -1.0f, 1.0f, 0.0f,
		/*bl*/ -1.0f, -1.0f, 0.0f, 0.0f,
		/*tl*/ -1.0f, +1.0f, 0.0f, 1.0f
	};
	gl_index_t attributes[] = {4};
	gl_index_t indices[] = {0, 3, 2, 0, 2, 1};
	mesh_StdRect = gl_createMesh((size2d_t){.height=4, .width=4}, 6, attributes, vertices, indices);

	/* Main process loop here */
	double lastFrameTime = 0;
	while(!gl_close(gl, -1)) {
		if (!source_read(source, texture_orginalFrame))
			gl_close(gl, 1);
		size2d_t windowSize = gl_drawStart(gl);

		gl_bindFrameBuffer(&framebuffer_stage1, (size2d_t){0,0}, 0);

		gl_useShader(&shader_filter3);
		gl_bindTexture(&texture_orginalFrame, 0);
		gl_drawMesh(&mesh_StdRect);

		gl_drawWindow(gl, &framebuffer_stage1.texture);

		char title[60];
		sprintf(title, "Viewer - frame %zu", frameCount++);
		gl_drawEnd(gl, title);
	}



	/* Free all resources, house keeping */
	status = EXIT_SUCCESS;
label_exit:
	gl_deleteMesh(&mesh_StdRect);
	gl_unloadShader(&shader_filter3);
	gl_deleteFrameBuffer(&framebuffer_stage1);
	
	gl_deleteTexture(&texture_orginalFrame);
	gl_close(gl, 1);
	gl_destroy(gl);
	source_destroy(source);

	fprintf(stdout, "%zu frames displayed.\n\n", frameCount);
	return status;
}