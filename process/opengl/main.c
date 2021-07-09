#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include "common.h"
#include "gl.h"

//#include "source.h"

#define WINDOW_RATIO 2

#define DEBUG_STAGE 1
#define DEBUG_FILE_DIR "./debugspace/debug.data"

typedef struct Thread_Source_DataStack {
	void* frameData;
	vh_t videoInfo;
	uint64_t readCount; //Writable for thread: -1 = initializing in progress, -2 = error, -3 = end
	uint64_t loadCount; //Writable for main: -1 = initializing in progress, -2 = halt signal
	/* Here we use a mailbox instead of mutex:
	| Thread updates readCount after it reads new frame frome file (buffer full and fresh);
	| Main updates loadCount after it loads frame into memory (buffer empty and free);
	| When readCount = loadCount: thread buffer empty and free, thread should read next frame, main should wait;
	| When readCount > loadCount: thread buffer full and fresh, thread should wait, main should load current frame.
	*/
} th_source_d;
#define TH_SOURCE_READ_INIT -1
#define TH_SOURCE_READ_ERROR -2
#define TH_SOURCE_READ_END -3
#define TH_SOURCE_LOAD_INIT -1
#define TH_SOURCE_LOAD_HALT -2
volatile th_source_d source_data = { .readCount = TH_SOURCE_READ_INIT, .loadCount = TH_SOURCE_LOAD_INIT };
void* thread_source(void* arg);

int main(int argc, char* argv[]) {
	int status = EXIT_FAILURE;

	size_t frameCount = 0;
	size2d_t size;
	
	GL gl = NULL;
	gl_obj texture_orginalFrame = 0;

	gl_fb framebuffer_stageA = {0, 0};
	gl_fb framebuffer_stageB = {0, 0};

	gl_obj shader_filter3 = 0;

	gl_mesh mesh_StdRect = {0, 0, 0, 0};

	/* Init source reading thread */
	pthread_t source_tid;
	pthread_attr_t source_attr;
	pthread_attr_init(&source_attr);
	pthread_attr_setdetachstate(&source_attr, PTHREAD_CREATE_JOINABLE);
	int source_error = pthread_create(&source_tid, &source_attr, thread_source, argv[1]);
	if (source_error) {
		fprintf(stderr, "Cannot init Source thread (%d).\n", source_error);
		goto label_exit;
	}

	while (source_data.readCount == TH_SOURCE_READ_INIT); //Waiting the source thread init
	if (source_data.readCount == TH_SOURCE_READ_ERROR) {
		fputs("Source thread error.\n", stderr);
		goto label_exit;
	}

	size = (size2d_t){.width = source_data.videoInfo.width, .height = source_data.videoInfo.height};

	/* Init OpenGL and viewer window */
	gl = gl_init(size, WINDOW_RATIO);
	if (!gl) {
		fputs("Cannot init OpenGL.\n", stderr);
		goto label_exit;
	}

	/* Use a texture to store raw frame data */
	texture_orginalFrame = gl_createTexture(size);

	/* Drawing on frame buffer to process data */
	framebuffer_stageA = gl_createFrameBuffer(size);
	framebuffer_stageB = gl_createFrameBuffer(size);

	/* Process - Kernel filter 3x3 */
	const char* shader_filter3_paramName[] = {"size", "maskTop", "maskMiddle", "maskBottom"};
	size_t shader_filter3_paramCount = sizeof(shader_filter3_paramName) / sizeof(shader_filter3_paramName[0]);
	gl_param shader_filter3_paramId[4];
	shader_filter3 = gl_loadShader("shader/stdRect.vs.glsl", "shader/filter3.fs.glsl", shader_filter3_paramName, shader_filter3_paramId, shader_filter3_paramCount);
	if (!shader_filter3) {
		fputs("Cannot load program.\n", stderr);
		goto label_exit;
	}
	gl_param shader_filter3_paramSize = shader_filter3_paramId[0];
	gl_param shader_filter3_paramMaskTop = shader_filter3_paramId[1];
	gl_param shader_filter3_paramMaskMiddle = shader_filter3_paramId[2];
	gl_param shader_filter3_paramMaskBottom = shader_filter3_paramId[3];
	gl_useShader(&shader_filter3);
	unsigned int shader_filter3_paramSize_v[] = {size.width, size.height}; //Cast to unsigned int first, need unsigned int, but size is of size_t
	gl_setShaderParam(shader_filter3_paramSize, 2, gl_type_uint, shader_filter3_paramSize_v); //Size of frame will not change

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

	/* Some extra code for development */
	gl_fb* frameBuffer_old = &framebuffer_stageA; //Double buffer in workspace, one as old, one as new
	gl_fb* frameBuffer_new = &framebuffer_stageB; //Swap the old and new after each stage using the function below
	void swapFrameBuffer() { /* GCC OK */
		gl_fb* temp = frameBuffer_old;
		frameBuffer_old = frameBuffer_new;
		frameBuffer_new = temp;
	}


	


	/* Main process loop here */
	double lastFrameTime = 0;
	source_data.loadCount = 0; //Main thread ready
	fputs("Main thread: ready\n", stdout);
	fflush(stdout);
	while(!gl_close(gl, -1)) {
		if (source_data.readCount == TH_SOURCE_READ_END) {
			gl_close(gl, 1);
			break;
		}
		while (source_data.readCount == source_data.loadCount);

		gl_drawStart(gl);

		gl_updateTexture(&texture_orginalFrame, size, source_data.frameData);
		gl_fsync(); //Force sync, make sure frame data is loaded
		source_data.loadCount++;
		
		/* Mailbox:
		| When readCount = loadCount: thread buffer empty and free, thread should read next frame, main should wait;
		| When readCount > loadCount: thread buffer full and fresh, thread should wait, main should load current frame.
		*/

		gl_bindFrameBuffer(frameBuffer_new, size, 0); //Use video full-resolution for render
		gl_useShader(&shader_filter3);
		const float gussianMask[] = {
			1.0f/16,	2.0f/16,	1.0f/16,
			2.0f/16,	4.0f/16,	2.0f/16,
			1.0f/16,	2.0f/16,	1.0f/16
		};
		gl_setShaderParam(shader_filter3_paramMaskTop, 3, gl_type_float, &(gussianMask[0]));
		gl_setShaderParam(shader_filter3_paramMaskMiddle, 3, gl_type_float, &(gussianMask[3]));
		gl_setShaderParam(shader_filter3_paramMaskBottom, 3, gl_type_float, &(gussianMask[6]));
		gl_bindTexture(&texture_orginalFrame, 0);
		gl_drawMesh(&mesh_StdRect);
		swapFrameBuffer();

		gl_bindFrameBuffer(frameBuffer_new, size, 0);
		gl_useShader(&shader_filter3);
		const float edgeMask[] = {
			1.0f,	1.0f,	1.0f,
			1.0f,	-8.0f,	1.0f,
			1.0f,	1.0f,	1.0f
		};
		gl_setShaderParam(shader_filter3_paramMaskTop, 3, gl_type_float, &(edgeMask[0]));
		gl_setShaderParam(shader_filter3_paramMaskMiddle, 3, gl_type_float, &(edgeMask[3]));
		gl_setShaderParam(shader_filter3_paramMaskBottom, 3, gl_type_float, &(edgeMask[6]));
		gl_bindTexture(&frameBuffer_old->texture, 0);
		gl_drawMesh(&mesh_StdRect);
		swapFrameBuffer();

		gl_drawWindow(gl, &frameBuffer_old->texture);

		char title[60];
		sprintf(title, "Viewer - frame %zu", frameCount++);
		gl_drawEnd(gl, title);
	}



	/* Free all resources, house keeping */
	status = EXIT_SUCCESS;
label_exit:
	gl_deleteMesh(&mesh_StdRect);
	gl_unloadShader(&shader_filter3);

	gl_deleteFrameBuffer(&framebuffer_stageB);
	gl_deleteFrameBuffer(&framebuffer_stageA);
	
	gl_deleteTexture(&texture_orginalFrame);
	gl_close(gl, 1);
	gl_destroy(gl);

	source_data.loadCount = TH_SOURCE_LOAD_HALT; //Send halt signal
	pthread_join(source_tid, NULL);

	fprintf(stdout, "%zu frames displayed.\n\n", frameCount);
	return status;
}



void* thread_source(void* arg) {
	/* Init */
	fputs("Source thread: Start!\n", stdout);
	fflush(stdout);

	FILE* fp = NULL;
	uint8_t* fileBuffer = NULL;
	uint8_t* workBuffer = NULL;
	vh_t info;

	/* Init */
	fp = fopen((char*)arg, "rb");
	if (!fp) {
		fprintf(stderr, "Source thread: Cannot open input file (errno = %d).\n", errno);
		goto thread_source_end_error;
	}

	if (!fread(&info, 1, sizeof(info), fp)) {
		fputs("Source thread: Cannot get file info.\n", stderr);
		goto thread_source_end_error;
	}
	if (info.colorScheme != 3) {
		fputs("Source thread: Bad color scheme.\n", stderr);
		goto thread_source_end_error;
	}

	fileBuffer = malloc(info.height * info.width * 3);
	workBuffer = malloc(info.height * info.width * 4);
	if (!fileBuffer || !workBuffer) {
		fputs("Source thread: Cannot allocated buffer.\n", stderr);
		goto thread_source_end_error;
	}

	source_data.videoInfo = info;
	source_data.frameData = workBuffer;

	/* Ready */
	fprintf(stdout, "Source thread: Ready (size=%"PRIu16"*%"PRIu16", cs=%"PRIu16", fps=%"PRIu16")!\n", info.width, info.height, info.colorScheme, info.fps);
	fflush(stdout);
	source_data.readCount = 0;
	while (source_data.loadCount != 0); //Waiting main thread ready
	
	/* Mailbox:
	| When readCount = loadCount: thread buffer empty and free, thread should read next frame, main should wait;
	| When readCount > loadCount: thread buffer full and fresh, thread should wait, main should load current frame.
	*/

	/* 3-stage buffer
	| There are 3 buffer: readBuffer, workBuffer, glBuffer
	| Read buffer: The fread() function put the raw file content into this buffer.
	| Work buffer: The program load data from readBuffer, process it, and save the processed, read-to-use data into the workBuffer.
	|		This is the buffer mentioned in the mailbox section.
	| GL buffer: This buffer is controled by the OpenGL driver. The GL driver decides when to DMA data from workBuffer to glBuffer.
	| ReadBuffer can be used again (override) after the program loads the data into workBuffer.
	| WorkBuffer can be used again (override) after the glFinish() or any other sync call.
	*/

	/* Loading frame data until reaching end of video file or main thread sending halt signal */
	while (1) {
		for (size_t i = 0; i < info.width * info.height; i++) { //Data format convert
			int color = (fileBuffer[i*3] + fileBuffer[i*3+1] + fileBuffer[i*3+2]) >> 2; //RGB->gray
			uint8_t* dest = source_data.frameData;
			dest[i*3+0] = fileBuffer[i*3+0];
			dest[i*3+1] = fileBuffer[i*3+1];
			dest[i*3+2] = fileBuffer[i*3+2];
		}

		source_data.readCount++;

		if (!fread(fileBuffer, 3, info.width * info.height, fp)) { //Reach the end of video file
			fputs("Source thread: End of file!\n", stdout);
			fflush(stdout);
			goto thread_source_end_ok;
		}
		
		while (1) { //Waiting main thread signal
			if (source_data.loadCount == TH_SOURCE_LOAD_HALT) {
				goto thread_source_end_ok;
			}
			if (source_data.loadCount >= source_data.readCount) {
				break;
			}
		}

	}
	

	/* End of video file or main thread sends halt signal */
	thread_source_end_ok:
	source_data.readCount = TH_SOURCE_READ_END; //Set end flag
	goto thread_source_end_quit;

	thread_source_end_error:
	source_data.readCount = TH_SOURCE_READ_ERROR; //Set error flag
	goto thread_source_end_quit;

	thread_source_end_quit:
	if (fp) fclose(fp); //fclose(NULL) undefined
	free(fileBuffer);
	free(workBuffer);

	fprintf(stdout, "Source thread: Halt! (err = %"PRIu64")\n", source_data.readCount + 3);
	fflush(stdout);
	return NULL;
}