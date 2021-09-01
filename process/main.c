#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>

#include "common.h"
#include "gl.h"

#define WINDOW_RATIO 1
#define MIXLEVEL 0.7f

#define FIFONAME "tmpframefifo.data"

//#define USE_PBO_UPLOAD

/* -- Reader thread ------------------------------------------------------------------------- */

volatile void volatile* rawDataPtr; //Video raw data goes here
sem_t sem_readerJobStart; //Fired by main thread: when pointer to pbo is ready, reader can begin to upload
sem_t sem_readerJobDone; //Fired by reader thread: when uploading is done, main thread can use
int sem_validFlag = 0;
enum sem_validFlag_Id {sem_validFlag_placeholder = 0, sem_validFlag_readerJobStart, sem_validFlag_readerJobDone};

struct th_reader_arg {
	size_t size; //Length of frame data in pixel
	int colorScheme; //Color scheme of the input raw data (1, 3 or 4)
};
void* th_reader(void* arg) {
	struct th_reader_arg* this = arg;
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

	unlink(FIFONAME); //Delete if exist
	if (mkfifo(FIFONAME, 0777) == -1) {
		fprintf(stderr, "[Reader] Fail to create FIFO '"FIFONAME"' (errno = %d)\n", errno);
		return NULL;
	}
	fputs("[Reader] Ready. FIFO '"FIFONAME"' can accept frame data now\n", stdout);
	fflush(stdout);

	FILE* fp = fopen(FIFONAME, "rb");
	if (!fp) {
		fprintf(stderr, "[Reader] Fail to open FIFO '"FIFONAME"' (errno = %d)\n", errno);
		unlink(FIFONAME);
		return NULL;
	}
	fputs("[Reader] FIFO '"FIFONAME"' data received\n", stdout);
	fflush(stdout);

	int fileValid = 1;
	while (fileValid) {
		sem_wait(&sem_readerJobStart); //Wait until main thread issue new GPU memory address for next frame

		const size_t bucketSize = 16; //Height and width are multiple of 4, so total size is multiple of 16. Read 16 pixels at once to increase performance

		size_t iter = this->size / bucketSize;
		uint8_t* dest = (void*)rawDataPtr;

		if (this->colorScheme == 1) {
			uint8_t luma[bucketSize];
			while (iter--) {
				uint8_t* p = &(luma[0]);
				if (!fread(p, 1, bucketSize, fp)) { //Fail to read from fifo
					fileValid = 0;
					break;
				}
				while (p < &( luma[16] )) {
					*(dest++) = *p; //R
					*(dest++) = *p; //G
					*(dest++) = *p; //B
					*(dest++) = 0xFF; //A
					p++;
				}
			}
		}
		else if (this->colorScheme == 3) {
			uint8_t rgb[bucketSize * 3];
			while (iter--) {
				uint8_t* p = &(rgb[0]);
				if (!fread(p, 3, bucketSize, fp)) {
					fileValid = 0;
					break;
				}
				while (p < &( rgb[bucketSize * 3] )) {
					*(dest++) = p[2]; //R
					*(dest++) = p[0]; //G
					*(dest++) = p[1]; //B
					*(dest++) = 0xFF; //A
					p += 3;
				}
			}
		}
		else if (this->colorScheme == 4) {
			uint8_t rgba[bucketSize * 4];
			while (iter--) {
				uint8_t* p = &(rgba[0]);
				if (!fread(rgba, 4, bucketSize, fp)) {
					fileValid = 0;
					break;
				}
				while (p < &( rgba[bucketSize * 4] )) {
					*(dest++) = p[2]; //R
					*(dest++) = p[0]; //G
					*(dest++) = p[1]; //B
					*(dest++) = p[3]; //A
					p += 4;
				}
			}
		}
		else {
			if (!fread(dest, 4, this->size, fp)) {
				fileValid = 0;
			}
		}

		fputc('R', stdout);
		sem_post(&sem_readerJobDone); //Uploading done, allow main thread to use it
	}

	fclose(fp);
	unlink(FIFONAME);
	fputs("[Reader] End of file or broken pipe! Please close the viewer window to terminate the program\n", stderr);

	while (1) { //Send dummy data to keep the main thread running
		sem_wait(&sem_readerJobStart); //Wait until main thread issue new GPU memory address for next frame
		memset((void*)rawDataPtr, 0, this->size * 4);
		sem_post(&sem_readerJobDone); //Uploading done, allow main thread to use it
	}

	return NULL;
}

/* -- Main thread --------------------------------------------------------------------------- */

int main(int argc, char* argv[]) {
	int status = EXIT_FAILURE;
	unsigned int frameCount = 0;

	/* Program argument check */
	if (argc != 5) {
		fputs("Bad arg: Use 'this width height fps color'\n\twhere color = 1(luma), 3(RGB) or 4(RGBA)", stderr);
		return status;
	}
	unsigned int width = atoi(argv[1]), height = atoi(argv[2]), fps = atoi(argv[3]), color = atoi(argv[4]);
	size2d_t size2d = {.width = width, .height = height};
	float fsize[2] = {width, height}; //Cast to float, for shader use, this value will never change
	size_t size1d = width * height; //Total size in pixel, for memory allocation use
	fprintf(stdout, "Video info = Width: %upx, Height: %upx, FPS: %u, Color: %u\n", width, height, fps, color);

	if (width & 0b11 || width < 320 || width > 4056) {
		fputs("Bad width: Width must be multiple of 4, 320 <= width <= 4056\n", stderr);
		return status;
	}
	if (height & 0b11 || height < 240 || height > 3040) {
		fputs("Bad height: Height must be multiple of 4, 240 <= height <= 3040\n", stderr);
		return status;
	}
	if (color != 1 && color != 3 && color != 4) {
		fputs("Bad color: Color must be 1, 2 or 4\n", stderr);
		return status;
	}

	/* Program variables declaration */

	//OpenGL root (OpenGL init, default frame buffer...)
	GL gl = NULL;

	//PBO or memory space for orginal video raw data uploading, and a texture to store orginal video data
	#ifdef USE_PBO_UPLOAD
		gl_pbo cameraData[2] = {GL_INIT_DEFAULT_PBO, GL_INIT_DEFAULT_PBO};
	#else
		void* rawData[2] = {NULL, NULL};
	#endif
	gl_tex texture_orginalBuffer = GL_INIT_DEFAULT_TEX;

	//Reader thread used to read orginal video raw data from external source
	pthread_t th_reader_id;
	int th_reader_idValid = 0;

	//Work buffer: Use off-screen render, one frame buffer as input data and one as output, an extra one to save previous frame
	gl_fb framebuffer_history = GL_INIT_DEFAULT_FB;
	gl_fb framebuffer_stageA = GL_INIT_DEFAULT_FB;
	gl_fb framebuffer_stageB = GL_INIT_DEFAULT_FB;
	gl_fb* fb_new;
	gl_fb* fb_old;

	//Program - Blit: Copy from one texture to another
	gl_shader shader_blit = GL_INIT_DEFAULT_SHADER;
	gl_param shader_blit_paramPStage;

	//Program - 3x3 Filter: 3x3 digital image filter
	gl_shader shader_filter3 = GL_INIT_DEFAULT_SHADER;
	gl_param shader_filter3_paramPStage;
	gl_param shader_filter3_blockMask;

	gl_shader shader_nonMaxSup = GL_INIT_DEFAULT_SHADER;
	gl_param shader_nonMaxSup_paramPStage;

	gl_shader shader_history = GL_INIT_DEFAULT_SHADER;
	gl_param shader_history_paramPStage;
	gl_param shader_history_paramHistory;

	const unsigned int bindingPoint_ubo_gussianMask =	0;
	const unsigned int bindingPoint_ubo_edgeMask =		1;
	gl_ubo shader_ubo_gussianMask = GL_INIT_DEFAULT_UBO;
	gl_ubo shader_ubo_edgeMask = GL_INIT_DEFAULT_UBO;

	gl_mesh mesh_StdRect = GL_INIT_DEFAULT_MESH;



	/* Init OpenGL and viewer window */ {
		gl = gl_init(size2d, WINDOW_RATIO, MIXLEVEL);
		if (!gl) {
			fputs("Cannot init OpenGL\n", stderr);
			goto label_exit;
		}
	}

	/* Use a texture to store raw frame data & Start reader thread */ {
		#ifdef USE_PBO_UPLOAD
			cameraData[0] = gl_pixelBuffer_create(size1d * 4); //Always use RGBA8 (good performance)
			cameraData[1] = gl_pixelBuffer_create(size1d * 4);
			if (cameraData[0] == GL_INIT_DEFAULT_PBO || cameraData[1] == GL_INIT_DEFAULT_PBO) {
				fputs("Fail to create pixel buffers for orginal frame data uploading\n", stderr);
				goto label_exit;
			}
		#else
			rawData[0] = malloc(size1d * 4);
			rawData[1] = malloc(size1d * 4);
			if (!rawData[0] || !rawData[1]) {
				fputs("Fail to create memory buffers for orginal frame data loading\n", stderr);
				goto label_exit;
			}
		#endif

		texture_orginalBuffer = gl_texture_create(gl_texformat_RGBA8, size2d);
		if (texture_orginalBuffer == GL_INIT_DEFAULT_TEX) {
			fputs("Fail to create texture buffer for orginal frame data storage\n", stderr);
			goto label_exit;
		}

		if (sem_init(&sem_readerJobStart, 0, 0)) {
			fprintf(stderr, "Fail to create main-reader master semaphore (errno = %d)\n", errno);
			goto label_exit;
		}
		sem_validFlag |= sem_validFlag_readerJobStart;
		if (sem_init(&sem_readerJobDone, 0, 0)) {
			fprintf(stderr, "Fail to create main-reader secondary semaphore (errno = %d)\n", errno);
			goto label_exit;
		}
		sem_validFlag |= sem_validFlag_readerJobDone;

		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
		struct th_reader_arg arg = (struct th_reader_arg){.size = size1d, .colorScheme = color};
		int err = pthread_create(&th_reader_id, &attr, th_reader, &arg);
		if (err) {
			fprintf(stderr, "Fail to init reader thread (%d)\n", err);
			goto label_exit;
		}
		th_reader_idValid = 1;
	}

	/* Create work buffer */ {
		framebuffer_history = gl_frameBuffer_create(gl_texformat_RGBA8, size2d);
/*		if (framebuffer_history == GL_INIT_DEFAULT_FB) {
			fputs("Fail to create frame buffer to store previous frames\n", stderr);
			goto label_exit;
		}*/
		framebuffer_stageA = gl_frameBuffer_create(gl_texformat_RGBA8, size2d);
		framebuffer_stageB = gl_frameBuffer_create(gl_texformat_RGBA8, size2d);
/*		if (framebuffer_stageA == GL_INIT_DEFAULT_FB || framebuffer_stageB == GL_INIT_DEFAULT_FB) {
			fputs("Fail to create work frame buffer A and/or B\n", stderr);
			goto label_exit;
		}*/
		fb_new = &framebuffer_stageA;
		fb_old = &framebuffer_stageB;
		#define swap(a, b) {gl_fb* temp = a; a = b; b = temp;}
	}

	/* Create program: Blit: Copy from one texture to another texture */ {
		const char* pName[] = {"size", "pStage"};
		unsigned int pCount = sizeof(pName) / sizeof(pName[0]);
		gl_param pId[pCount];

		const char* bName[] = {};
		unsigned int bCount = sizeof(bName) / sizeof(bName[0]);
		gl_param bId[bCount];

		shader_blit = gl_shader_load("shader/stdRect.vs.glsl", "shader/blit.fs.glsl", 1, pName, pId, pCount, bName, bId, bCount);
		if (shader_blit == GL_INIT_DEFAULT_SHADER) {
			fputs("Cannot load shader: blit\n", stderr);
			goto label_exit;
		}

		gl_shader_use(&shader_blit);
		gl_shader_setParam(pId[0], 2, gl_type_float, fsize);

		shader_blit_paramPStage = pId[1];
	}

	/* Create program: 3x3 Filter */ {
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
/*		Roadmap roadmap = roadmap_init(argv[2]);
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
		roadmap_destroy(roadmap); //Free roadmap memory after data uploading finished*/

		gl_vertex_t vertices[] = {
			1.0f, 1.0f,
			1.0f, 0.0f,
			0.0f, 0.0f,
			0.0f, 1.0f
		};
		gl_index_t attributes[] = {2};
		gl_index_t indices[] = {0, 3, 2, 0, 2, 1};
		mesh_StdRect = gl_mesh_create((size2d_t){.height=4, .width=2}, 6, attributes, vertices, indices);

	}

	
	

	void ISR_SIGINT() {
		gl_close(gl, 1);
	}
	signal(SIGINT, ISR_SIGINT);

	/* Main process loop here */
	uint64_t startTime, endTime;
	while(!gl_close(gl, -1)) {
		fputc('\r', stdout);
		startTime = nanotime();
		gl_drawStart(gl);
		
		/* Give next frame PBO to reader while using current PBO for rendering */
		#ifdef USE_PBO_UPLOAD
			gl_pixelBuffer_updateToTexture(gl_texformat_RGBA8, &cameraData[ (frameCount+0)&1 ], &texture_orginalBuffer, size2d); //Current frame PBO (index = +0) <-- Data already in GPU, this operation is fast
			rawDataPtr = gl_pixelBuffer_updateStart(&cameraData[ (frameCount+1)&1 ], size1d * 4); //Next frame PBO (index = +1)
		#else
			gl_texture_update(gl_texformat_RGBA8, &texture_orginalBuffer, size2d, rawData[ (frameCount+0)&1 ]);
			rawDataPtr = rawData[ (frameCount+1)&1 ];
		#endif
		sem_post(&sem_readerJobStart); //New GPU address ready, start to reader thread

		/* Remove noise by using gussian blur mask */
		gl_frameBuffer_bind(fb_new, size2d, 0); //Use video full-resolution for render
		gl_shader_use(&shader_filter3);
		gl_uniformBuffer_bindShader(bindingPoint_ubo_gussianMask, &shader_filter3, shader_filter3_blockMask);
		gl_texture_bind(&texture_orginalBuffer, shader_filter3_paramPStage, 0);
		gl_mesh_draw(&mesh_StdRect);
		swap(fb_new, fb_old);

		/* Apply edge detection mask */
		gl_frameBuffer_bind(fb_new, size2d, 0);
		gl_shader_use(&shader_filter3);
		gl_uniformBuffer_bindShader(bindingPoint_ubo_edgeMask, &shader_filter3, shader_filter3_blockMask);
		gl_texture_bind(&fb_old->texture, shader_filter3_paramPStage, 0);
		gl_mesh_draw(&mesh_StdRect);
		swap(fb_new, fb_old);

/*		gl_frameBuffer_bind(frameBuffer_new, size, 0);
		gl_shader_use(&shader_history);
		gl_texture_bind(&frameBuffer_old->texture, shader_history_paramPStage, 0);
		gl_texture_bind(&framebuffer_history.texture, shader_history_paramHistory, 1);
		gl_mesh_draw(&mesh_StdRect);
		swapFrameBuffer(frameBuffer_old, frameBuffer_new);

		gl_frameBuffer_bind(&framebuffer_history, size, 0); //Render to history framebuffer instead frameBuffer_new
		gl_shader_use(&shader_blit); //So, DON'T swap framebuffer after rendering
		gl_texture_bind(&frameBuffer_old->texture, shader_blit_paramPStage, 0);
		gl_mesh_draw(&mesh_StdRect);*/

		gl_drawWindow(gl, &texture_orginalBuffer, &fb_old->texture);

		char title[60];
		sprintf(title, "Viewer - frame %u", frameCount);
		gl_drawEnd(gl, title);

		fputc('M', stdout);
		sem_wait(&sem_readerJobDone); //Wait reader thread finish uploading frame data
		#ifdef USE_PBO_UPLOAD
			gl_pixelBuffer_updateFinish();
		#endif

		endTime = nanotime();
		fprintf(stdout, "Loop %u takes %lfms/frame.", frameCount, (endTime - startTime) / 1e6);
		fflush(stdout);
	
		frameCount++;
//		usleep(10e3);
	}



	/* Free all resources, house keeping */
	status = EXIT_SUCCESS;
label_exit:
	gl_mesh_delete(&mesh_StdRect);
	
	gl_unifromBuffer_delete(&shader_ubo_gussianMask);
	gl_unifromBuffer_delete(&shader_ubo_edgeMask);

	gl_shader_unload(&shader_history);
	gl_shader_unload(&shader_filter3);
	gl_shader_unload(&shader_nonMaxSup);
	gl_shader_unload(&shader_blit);

	gl_frameBuffer_delete(&framebuffer_stageB);
	gl_frameBuffer_delete(&framebuffer_stageA);
	gl_frameBuffer_delete(&framebuffer_history);

	if (th_reader_idValid) {
		pthread_cancel(th_reader_id);
		pthread_join(th_reader_id, NULL);
	}
	th_reader_idValid = 0;

	if (sem_validFlag & sem_validFlag_readerJobStart)
		sem_destroy(&sem_readerJobStart);
	if (sem_validFlag & sem_validFlag_readerJobDone)
		sem_destroy(&sem_readerJobDone);

	gl_texture_delete(&texture_orginalBuffer);
	#ifdef USE_PBO_UPLOAD
		gl_pixelBuffer_delete(&cameraData[1]);
		gl_pixelBuffer_delete(&cameraData[0]);
	#else
		free(rawData[0]);
		free(rawData[1]);
	#endif

	gl_destroy(gl);

	fprintf(stdout, "\n%u frames displayed.\n\n", frameCount);
	return status;
}