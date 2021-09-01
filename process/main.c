#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include "common.h"
#include "gl.h"
#include "mt_source.h"
#include "roadmap.h"

#define WINDOW_RATIO 2

int main(int argc, char* argv[]) {
	int status = EXIT_FAILURE;

	size_t frameCount = 0;
	
	MT_Source source = NULL;
	size2d_t size;
	float fsize[2]; //Cast to float, for shader use, this value will never change
	size_t tsize; //Total size in pixel, for memory allocation use
	unsigned int fps;

	GL gl = NULL;

	gl_tex texture_orginalFrame = GL_INIT_DEFAULT_TEX;
	gl_fb framebuffer_history = GL_INIT_DEFAULT_FB;
	gl_fb framebuffer_stageA = GL_INIT_DEFAULT_FB;
	gl_fb framebuffer_stageB = GL_INIT_DEFAULT_FB;

	gl_shader shader_blit = GL_INIT_DEFAULT_SHADER;
	gl_param shader_blit_paramPStage;

	gl_shader shader_filter3 = GL_INIT_DEFAULT_SHADER;
	gl_param shader_filter3_paramPStage;
	gl_param shader_filter3_blockMask;

	gl_shader shader_history = GL_INIT_DEFAULT_SHADER;
	gl_param shader_history_paramPStage;
	gl_param shader_history_paramHistory;

	const unsigned int bindingPoint_ubo_gussianMask =	0;
	const unsigned int bindingPoint_ubo_edgeMask =		1;
	gl_ubo shader_ubo_gussianMask = GL_INIT_DEFAULT_UBO;
	gl_ubo shader_ubo_edgeMask = GL_INIT_DEFAULT_UBO;

	gl_mesh mesh_StdRect = GL_INIT_DEFAULT_MESH;

	/* Init source reading thread */ {
		source = mt_source_init(argv[1]);
		if (!source) {
			fputs("Cannot init MT-Source class object (Source reading thread).\n", stderr);
			goto label_exit;
		}

		vh_t info = mt_source_getInfo(source);
		fps = info.fps;
		size = (size2d_t){.width = info.width, .height = info.height};
		fsize[0] = size.width;
		fsize[1] = size.height;
		tsize = size.width * size.height;
	}


	/* Init OpenGL and viewer window */
	gl = gl_init(size, WINDOW_RATIO);
	if (!gl) {
		fputs("Cannot init OpenGL.\n", stderr);
		goto label_exit;
	}

	/* Use a texture to store raw frame data */
	texture_orginalFrame = gl_texture_create(size);

	/* Use a texture to remember previous edges */
	framebuffer_history = gl_frameBuffer_create(size); //First few frames will contain garbages, but we are OK with that

	/* Drawing on frame buffer to process data */
	framebuffer_stageA = gl_frameBuffer_create(size);
	framebuffer_stageB = gl_frameBuffer_create(size);

	/* Util - Blitting texture in framebuffer */ {
		const char* pName[] = {"size", "pStage"};
		unsigned int pCount = sizeof(pName) / sizeof(pName[0]);
		gl_param pId[pCount];

		const char* bName[] = {};
		unsigned int bCount = sizeof(bName) / sizeof(bName[0]);
		gl_param bId[bCount];

		shader_blit = gl_shader_load("shader/stdRect.vs.glsl", "shader/blit.fs.glsl", 1, pName, pId, pCount, bName, bId, bCount);
		if (shader_blit == GL_INIT_DEFAULT_SHADER) {
			fputs("Cannot load shader: 3*3 filter\n", stderr);
			goto label_exit;
		}

		gl_shader_use(&shader_blit);
		gl_shader_setParam(pId[0], 2, gl_type_float, fsize);

		shader_blit_paramPStage = pId[1];
	}

	/* Process - Kernel filter 3x3 */ {
		const char* pName[] = {"size", "pStage"};
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

		gl_shader_use(&shader_filter3);
		gl_shader_setParam(pId[0], 2, gl_type_float, fsize);

		shader_filter3_paramPStage = pId[1];
		shader_filter3_blockMask = bId[0];
	}

	/* Process - Check pervious frames */ {
		const char* pName[] = {"size", "pStage", "history"};
		unsigned int pCount = sizeof(pName) / sizeof(pName[0]);
		gl_param pId[pCount];

		const char* bName[] = {};
		unsigned int bCount = sizeof(bName) / sizeof(bName[0]);
		gl_param bId[bCount];

		shader_history = gl_shader_load("shader/stdRect.vs.glsl", "shader/history.fs.glsl", 1, pName, pId, pCount, bName, bId, bCount);
		if (shader_history == GL_INIT_DEFAULT_SHADER) {
			fputs("Cannot load shader: History frame lookback\n", stderr);
			goto label_exit;
		}

		gl_shader_use(&shader_history);
		gl_shader_setParam(pId[0], 2, gl_type_float, fsize);

		shader_history_paramPStage = pId[1];
		shader_history_paramHistory = pId[2];
	}

	/* Process - Kernel filter 3*3 masks */ {
		const float gussianMask[] = {
			1.0f/16,	2.0f/16,	1.0f/16,	0.0f, //vec4 alignment
			2.0f/16,	4.0f/16,	2.0f/16,	0.0f,
			1.0f/16,	2.0f/16,	1.0f/16,	0.0f
		};
		shader_ubo_gussianMask = gl_uniformBuffer_create(bindingPoint_ubo_gussianMask, sizeof(gussianMask));
		gl_uniformBuffer_update(&shader_ubo_gussianMask, 0, sizeof(gussianMask), gussianMask);

		const float edgeMask[] = {
			1.0f,	1.0f,	1.0f,	0.0f,
			1.0f,	-8.0f,	1.0f,	0.0f,
			1.0f,	1.0f,	1.0f,	0.0f
		};
		shader_ubo_edgeMask = gl_uniformBuffer_create(bindingPoint_ubo_edgeMask, sizeof(edgeMask));
		gl_uniformBuffer_update(&shader_ubo_edgeMask, 0, sizeof(edgeMask), edgeMask);
	}

	/* Drawing mash (simple rect) */ {
		Roadmap roadmap = roadmap_init(argv[2]);
		if (!roadmap) {
			fputs("Cannot load roadmap\n", stderr);
			goto label_exit;
		}

		size_t vCount, iCount;
		gl_vertex_t* vertices = roadmap_getVertices(roadmap, &vCount);
		gl_index_t* indices = roadmap_getIndices(roadmap, &iCount);
		gl_index_t attributes[] = {2, 2};
		mesh_StdRect = gl_mesh_create((size2d_t){.height=vCount, .width=4}, 3 * iCount, attributes, vertices, indices);

		gl_fsync();
		roadmap_destroy(roadmap); //Free roadmap memory after data uploading finished

/*		gl_vertex_t vertices[] = {
			1.0f, 1.0f,
			1.0f, 0.0f,
			0.0f, 0.0f,
			0.0f, 1.0f
		};
		gl_index_t attributes[] = {2};
		gl_index_t indices[] = {0, 3, 2, 0, 2, 1};
		mesh_StdRect = gl_mesh_create((size2d_t){.height=4, .width=2}, 6, attributes, vertices, indices);*/

	}

	/* Some extra code for development */
	gl_fb* frameBuffer_old = &framebuffer_stageA; //Double buffer in workspace, one as old, one as new
	gl_fb* frameBuffer_new = &framebuffer_stageB; //Swap the old and new after each stage using the function below
	gl_fb* temp;
	#define swapFrameBuffer(a, b) {gl_fb* temp = a; a = b; b = temp;}

	void ISR_SIGINT() {
		gl_close(gl, 1);
	}
	signal(SIGINT, ISR_SIGINT);

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

		/* Remove noise by using gussian blur mask */
		gl_frameBuffer_bind(frameBuffer_new, size, 0); //Use video full-resolution for render
		gl_shader_use(&shader_filter3);
		gl_uniformBuffer_bindShader(bindingPoint_ubo_gussianMask, &shader_filter3, shader_filter3_blockMask);
		gl_texture_bind(&texture_orginalFrame, shader_filter3_paramPStage, 0);
		gl_mesh_draw(&mesh_StdRect);
		swapFrameBuffer(frameBuffer_old, frameBuffer_new);

		/* Apply edge detection mask */
		gl_frameBuffer_bind(frameBuffer_new, size, 0);
		gl_shader_use(&shader_filter3);
		gl_uniformBuffer_bindShader(bindingPoint_ubo_edgeMask, &shader_filter3, shader_filter3_blockMask);
		gl_texture_bind(&frameBuffer_old->texture, shader_filter3_paramPStage, 0);
		gl_mesh_draw(&mesh_StdRect);
		swapFrameBuffer(frameBuffer_old, frameBuffer_new);

		gl_frameBuffer_bind(frameBuffer_new, size, 0);
		gl_shader_use(&shader_history);
		gl_texture_bind(&frameBuffer_old->texture, shader_history_paramPStage, 0);
		gl_texture_bind(&framebuffer_history.texture, shader_history_paramHistory, 1);
		gl_mesh_draw(&mesh_StdRect);
		swapFrameBuffer(frameBuffer_old, frameBuffer_new);

		gl_frameBuffer_bind(&framebuffer_history, size, 0); //Render to history framebuffer instead frameBuffer_new
		gl_shader_use(&shader_blit); //So, DON'T swap framebuffer after rendering
		gl_texture_bind(&frameBuffer_old->texture, shader_blit_paramPStage, 0);
		gl_mesh_draw(&mesh_StdRect);

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
	
	gl_unifromBuffer_delete(&shader_ubo_gussianMask);
	gl_unifromBuffer_delete(&shader_ubo_edgeMask);

	gl_shader_unload(&shader_history);
	gl_shader_unload(&shader_filter3);
	gl_shader_unload(&shader_blit);

	gl_frameBuffer_delete(&framebuffer_stageB);
	gl_frameBuffer_delete(&framebuffer_stageA);
	gl_frameBuffer_delete(&framebuffer_history);
	gl_texture_delete(&texture_orginalFrame);

	gl_destroy(gl);

	mt_source_destroy(source);

	fprintf(stdout, "\n%zu frames displayed.\n\n", frameCount);
	return status;
}