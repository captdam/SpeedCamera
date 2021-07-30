#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <errno.h>

#include "common.h"
#include "gl.h"
#include "mt_source.h"

//#include "source.h"

#define WINDOW_RATIO 2

#define DEBUG_STAGE 1
#define DEBUG_FILE_DIR "./debugspace/debug.data"



int main(int argc, char* argv[]) {
	int status = EXIT_FAILURE;

	size_t frameCount = 0;
	
	MT_Source source = NULL;
	size2d_t size;

	GL gl = NULL;

	gl_tex texture_orginalFrame = GL_INIT_DEFAULT_TEX;
	gl_fb framebuffer_stageA = GL_INIT_DEFAULT_FB;
	gl_fb framebuffer_stageB = GL_INIT_DEFAULT_FB;

	gl_shader shader_filter3 = GL_INIT_DEFAULT_SHADER;
	gl_param shader_filter3_paramSize;
	gl_param shader_filter3_blockMask;

	unsigned int ubo_bp_gussianMask =	0;
	unsigned int ubo_bp_edgeMask =		1;
	gl_ubo shader_ubo_gussianMask = GL_INIT_DEFAULT_UBO;
	gl_ubo shader_ubo_edgeMask = GL_INIT_DEFAULT_UBO;

//	gl_shader shader_history = GL_INIT_DEFAULT_SHADER;
//	gl_ssbo shader_history_ssboPreviousFrame = GL_INIT_DEFAULT_SSBO;

	gl_mesh mesh_StdRect = GL_INIT_DEFAULT_MESH;

	/* Init source reading thread */
	source = mt_source_init(argv[1]);
	if (!source) {
		fputs("Cannot init MT-Source class object (Source reading thread).\n", stderr);
		goto label_exit;
	}
	size = mt_source_getSize(source);

	/* Init OpenGL and viewer window */
	gl = gl_init(size, WINDOW_RATIO);
	if (!gl) {
		fputs("Cannot init OpenGL.\n", stderr);
		goto label_exit;
	}

	/* Use a texture to store raw frame data */
	texture_orginalFrame = gl_texture_create(size);

	/* Drawing on frame buffer to process data */
	framebuffer_stageA = gl_frameBuffer_create(size);
	framebuffer_stageB = gl_frameBuffer_create(size);

	/* Process - Kernel filter 3x3 */ {
		const char* pName[] = {"size"};
		unsigned int pCount = sizeof(pName) / sizeof(pName[0]);
		gl_param pId[pCount];

		const char* bName[] = {"FilterMask"};
		unsigned int bCount = sizeof(bName) / sizeof(bName[0]);
		gl_param bId[bCount];

		shader_filter3 = gl_shader_load("shader/stdRect.vs.glsl", "shader/filter3.fs.glsl", 1, pName, pId, pCount, bName, bId, bCount);
		if (shader_filter3 == GL_INIT_DEFAULT_SHADER) {
			fputs("Cannot load shader: 3*3 filter\n", stderr);
			goto label_exit;
		}

		shader_filter3_paramSize = pId[0];
		shader_filter3_blockMask = bId[0];

		gl_shader_use(&shader_filter3);
		float shader_filter3_paramSize_v[] = {size.width, size.height}; //Cast to float
		gl_shader_setParam(shader_filter3_paramSize, 2, gl_type_float, shader_filter3_paramSize_v); //Size of frame will never change
	}

	/* Process - Check pervious frame */
/*	shader_history = gl_shader_load("shader/stdRect.vs.glsl", "shader/history.fs.glsl", 1, NULL, NULL, 0);
	if (shader_history == GL_INIT_DEFAULT_SHADER) {
		fputs("Cannot load shader: History frame lookback\n", stderr);
		goto label_exit;
	}*/

	/* Process - Kernel filter 3*3 masks */ {
		const float gussianMask[] = {
			1.0f/16,	2.0f/16,	1.0f/16,	0.0f, //vec4 alignment
			2.0f/16,	4.0f/16,	2.0f/16,	0.0f,
			1.0f/16,	2.0f/16,	1.0f/16,	0.0f
		};
		shader_ubo_gussianMask = gl_uniformBuffer_create(ubo_bp_gussianMask, sizeof(gussianMask));
		gl_uniformBuffer_update(&shader_ubo_gussianMask, 0, sizeof(gussianMask), gussianMask);

		const float edgeMask[] = {
			1.0f,	1.0f,	1.0f,	0.0f,
			1.0f,	-8.0f,	1.0f,	0.0f,
			1.0f,	1.0f,	1.0f,	0.0f
		};
		shader_ubo_edgeMask = gl_uniformBuffer_create(ubo_bp_edgeMask, sizeof(edgeMask));
		gl_uniformBuffer_update(&shader_ubo_edgeMask, 0, sizeof(edgeMask), edgeMask);
	}

	/* Drawing mash (simple rect) */ {
		gl_vertex_t vertices[] = {
			/*tr*/ 1.0f, 1.0f,
			/*br*/ 1.0f, 0.0f,
			/*bl*/ 0.0f, 0.0f,
			/*tl*/ 0.0f, 1.0f
		};
		gl_index_t attributes[] = {2};
		gl_index_t indices[] = {0, 3, 2, 0, 2, 1};
		mesh_StdRect = gl_mesh_create((size2d_t){.height=4, .width=2}, 6, attributes, vertices, indices);
	}

	/* Some extra code for development */
	gl_fb* frameBuffer_old = &framebuffer_stageA; //Double buffer in workspace, one as old, one as new
	gl_fb* frameBuffer_new = &framebuffer_stageB; //Swap the old and new after each stage using the function below
	gl_fb* temp;
	#define swapFrameBuffer(a, b) {gl_fb* temp = a; a = b; b = temp;}

	/* Main process loop here */
	fputs("Main thread: ready\n", stdout);
	fflush(stdout);
	uint64_t startTime, endTime;
	while(!gl_close(gl, -1)) {
		startTime = nanotime();

		void* frame = mt_source_start(source); //Poll frame data from source class
		if (!frame) {
			gl_close(gl, 1);
			break;
		}
		
		gl_drawStart(gl);

		gl_texture_update(&texture_orginalFrame, size, frame);

		gl_frameBuffer_bind(frameBuffer_new, size, 0); //Use video full-resolution for render
		gl_shader_use(&shader_filter3);
		gl_uniformBuffer_bindShader(ubo_bp_gussianMask, &shader_filter3, shader_filter3_blockMask);
		gl_texture_bind(&texture_orginalFrame, 0);
		gl_mesh_draw(&mesh_StdRect);
		swapFrameBuffer(frameBuffer_old, frameBuffer_new);

		gl_frameBuffer_bind(frameBuffer_new, size, 0);
		gl_shader_use(&shader_filter3);
		gl_uniformBuffer_bindShader(ubo_bp_edgeMask, &shader_filter3, shader_filter3_blockMask);
		gl_texture_bind(&frameBuffer_old->texture, 0);
		gl_mesh_draw(&mesh_StdRect);
		swapFrameBuffer(frameBuffer_old, frameBuffer_new);

/*		gl_frameBuffer_bind(frameBuffer_new, size, 0);
		gl_shader_use(&shader_history);
		gl_texture_bind(&frameBuffer_old->texture, 0);
		gl_mesh_draw(&mesh_StdRect);
		swapFrameBuffer(frameBuffer_old, frameBuffer_new);*/

		gl_drawWindow(gl, &texture_orginalFrame, &frameBuffer_old->texture); //A forced opengl synch
		mt_source_finish(source); //Release source class buffer

		char title[60];
		sprintf(title, "Viewer - frame %zu", frameCount++);
		gl_drawEnd(gl, title);

		endTime = nanotime();
		fprintf(stdout, "\rLoop %zu takes %lfms/frame.", frameCount, (endTime - startTime) / 1e6);
		fflush(stdout);
	}



	/* Free all resources, house keeping */
	status = EXIT_SUCCESS;
label_exit:
	gl_mesh_delete(&mesh_StdRect);
	
//	gl_shaderStorageBuffer_delete(&shader_history_ssboPreviousFrame);
//	gl_shader_unload(&shader_history);
	
	gl_unifromBuffer_delete(&shader_ubo_gussianMask);
	gl_unifromBuffer_delete(&shader_ubo_edgeMask);

	gl_shader_unload(&shader_filter3);

	gl_frameBuffer_delete(&framebuffer_stageB);
	gl_frameBuffer_delete(&framebuffer_stageA);
	gl_texture_delete(&texture_orginalFrame);

	gl_destroy(gl);

	mt_source_destroy(source);

	fprintf(stdout, "\n%zu frames displayed.\n\n", frameCount);
	return status;
}