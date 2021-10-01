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
#include "roadmap.h"
#include "speedometer.h"

#define WINDOW_RATIO 1
#define MIXLEVEL 0.93f //0.88f
#define FIVE_SECONDS_IN_NS 5000000000LLU

#define FIFONAME "tmpframefifo.data"

#define RAW_COLORFORMAT gl_texformat_RGBA8

#define SPEEDOMETER_FILE "./textmap.data"
#define SPEEDOMETER_SIZE (size2d_t){.x = 48 * 16, .y = 24 * 16}
#define SPEEDOMETER_COUNT (size2d_t){.x = 1, .y = 1}

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

		fputc('R', stdout); fflush(stdout);
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
	if (argc != 7) {
		fputs(
			"Bad arg: Use 'this width height fps color focusRegionFile distanceMapFile'\n"
			"\twhere color = 1(luma), 3(RGB) or 4(RGBA)\n"
			"\t      focusRegionFile = Directory to a human-readable file contains focus region\n"
			"\t      distanceMapFile = Directory to a binary coded file contains road-domain geographic data\n"
		, stderr);
		return status;
	}
	unsigned int width = atoi(argv[1]), height = atoi(argv[2]), fps = atoi(argv[3]), color = atoi(argv[4]);
	const char* focusRegionFile = argv[5];
	const char* distanceMapFile = argv[6];
	size2d_t size2d = {.width = width, .height = height}; //Number of pixels in width and height
	float fsize[2] = {width, height}; //Number of pixels in width[0] and height[1], cast to float for shader use
	size_t size1d = width * height; //Total number of in pixels = width * height
	fprintf(stdout, "Video info = Width: %upx, Height: %upx, Total: %zu, FPS: %u, Color: %u\n", width, height, size1d, fps, color);
	fprintf(stdout, "\tFocus region: %s\n", focusRegionFile);
	fprintf(stdout, "\tDistance map: %s\n", distanceMapFile);

	if ((unsigned int)width & (unsigned int)0b11 || width < 320 || width > 4056) {
		fputs("Bad width: Width must be multiple of 4, 320 <= width <= 4056\n", stderr);
		return status;
	}
	if ((unsigned int)height & (unsigned int)0b11 || height < 240 || height > 3040) {
		fputs("Bad height: Height must be multiple of 4, 240 <= height <= 3040\n", stderr);
		return status;
	}
	if (color != 1 && color != 3 && color != 4) {
		fputs("Bad color: Color must be 1, 2 or 4\n", stderr);
		return status;
	}

	/* Program variables declaration */

	//Max distance an object could possibly travel between two frame
	float distanceThreshold;

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
	struct th_reader_arg th_reader_arg;
	int th_reader_idValid = 0;

	//A mesh to store focus region, and a texture to store road-domain data
	gl_mesh mesh_region = GL_INIT_DEFAULT_MESH;
	gl_tex texture_road = GL_INIT_DEFAULT_TEX;

	//To display human readable text on screen
	const char* speedometer_filename = SPEEDOMETER_FILE;
	size2d_t speedometer_size = SPEEDOMETER_SIZE;
	size2d_t speedometer_count = SPEEDOMETER_COUNT;
	float speedometer_sizeFloat[2] = {speedometer_size.width, speedometer_size.height};
	gl_mesh mesh_speedometer = GL_INIT_DEFAULT_MESH;
	gl_tex texture_speedometer = GL_INIT_DEFAULT_TEX;

	//Work stages: To save intermediate result during the process
	gl_fb framebuffer_edge[2] = {GL_INIT_DEFAULT_FB, GL_INIT_DEFAULT_FB}; //Edge of current and previous frame
	gl_fb framebuffer_move[2] = {GL_INIT_DEFAULT_FB, GL_INIT_DEFAULT_FB}; //Moving edge of current and previous frame
	gl_fb framebuffer_speed = GL_INIT_DEFAULT_FB; //Displacement of two moving edge (speed of moving edge)

	//Work buffer: Temp buffer
	gl_fb framebuffer_stageA = GL_INIT_DEFAULT_FB;
	gl_fb framebuffer_stageB = GL_INIT_DEFAULT_FB;

	//Program[Debug] - Roadmap check: Roadmap match with video
	gl_shader shader_roadmapCheck = GL_INIT_DEFAULT_SHADER;
	gl_param shader_roadmapCheck_paramRoadmap;

	//Program - Blit: Copy from one texture to another
	gl_shader shader_blit = GL_INIT_DEFAULT_SHADER;
	gl_param shader_blit_paramPStage;

	//Program - 3x3 Filter: 3x3 digital image filter
	gl_shader shader_filter3 = GL_INIT_DEFAULT_SHADER;
	gl_param shader_filter3_paramPStage;
	gl_param shader_filter3_blockMask;

	//Program - Edge refine: Refine the result of edge detection
	gl_shader shader_edgerefine = GL_INIT_DEFAULT_SHADER;
	gl_param shader_edgerefine_paramPStage;

	//Program - Compare different: Find different between two textures
	gl_shader shader_different = GL_INIT_DEFAULT_SHADER;
	gl_param shader_different_paramA;
	gl_param shader_different_paramB;

	//Program - Distance: Find the distance of moving edge
	gl_shader shader_distance = GL_INIT_DEFAULT_SHADER;
	gl_param shader_distance_paramRoadmap;
	gl_param shader_distance_paramA;
	gl_param shader_distance_paramB;

	//Program - Speedometer: Display human-readable text representing speed
	gl_shader shader_speedometer = GL_INIT_DEFAULT_SHADER;
	gl_param shader_speedometer_paramPStage;
	gl_param shader_speedometer_paramGlyphTexture;

	//Param - 3x3 filter masks
	const unsigned int bindingPoint_ubo_gussianMask =	0;
	const unsigned int bindingPoint_ubo_edgeMask =		1;
	gl_ubo shader_ubo_gussianMask = GL_INIT_DEFAULT_UBO;
	gl_ubo shader_ubo_edgeMask = GL_INIT_DEFAULT_UBO;



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
			if (!gl_pixelBuffer_check(&(cameraData[0])) || !gl_pixelBuffer_check(&(cameraData[1]))) {
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

		texture_orginalBuffer = gl_texture_create(RAW_COLORFORMAT, size2d);
		if (!gl_texture_check(&texture_orginalBuffer)) {
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
		th_reader_arg = (struct th_reader_arg){.size = size1d, .colorScheme = color};
		int err = pthread_create(&th_reader_id, &attr, th_reader, &th_reader_arg);
		if (err) {
			fprintf(stderr, "Fail to init reader thread (%d)\n", err);
			goto label_exit;
		}
		th_reader_idValid = 1;
	}

	/* Roadmap: mesh for focus region and texture for road info */ {
		Roadmap roadmap = roadmap_init(focusRegionFile, distanceMapFile, size1d);
		if (!roadmap) {
			fputs("Cannot load roadmap\n", stderr);
			goto label_exit;
		}
		
		distanceThreshold = roadmap_getThreshold(roadmap);
		fprintf(stdout, "Roadmap threshold: %f/frame\n", distanceThreshold);

		size_t vCount, iCount;
		gl_vertex_t* vertices = roadmap_getVertices(roadmap, &vCount);
		gl_index_t* indices = roadmap_getIndices(roadmap, &iCount);
		gl_index_t attributes[] = {2};
		mesh_region = gl_mesh_create((size2d_t){.height=vCount, .width=2}, 3 * iCount, attributes, vertices, indices);
		if (!gl_mesh_check(&mesh_region)) {
			roadmap_destroy(roadmap);
			fputs("Fail to create mesh for roadmap - focus region\n", stderr);
			goto label_exit;
		}

		texture_road = gl_texture_create(gl_texformat_RGBA16F, size2d);
		if (!gl_texture_check(&texture_road)) {
			roadmap_destroy(roadmap);
			fputs("Fail to create texture buffer for roadmap - road-domain data storage\n", stderr);
			goto label_exit;
		}

		gl_texture_update(gl_texformat_RGBA16F, &texture_road, size2d, roadmap_getGeographic(roadmap));
		gl_synch barrier = gl_synchSet();
		if (gl_synchWait(barrier, FIVE_SECONDS_IN_NS) == gl_synch_timeout) {
			fputs("Roadmap GPU uploading fatal stall\n", stderr);
			gl_synchDelete(barrier);
			roadmap_destroy(roadmap);
			goto label_exit;
		}
		gl_synchDelete(barrier);

		roadmap_destroy(roadmap); //Free roadmap memory after data uploading finished

	}

	/* Load speedometer texture: a text bitmap */ {
		Speedometer speedometer = speedometer_init(speedometer_filename, speedometer_size, speedometer_count);
		if (!speedometer) {
			fputs("Cannot load speedometer\n", stderr);
			goto label_exit;
		}

		size_t vCount, iCount;
		gl_vertex_t* vertices = speedometer_getVertices(speedometer, &vCount);
		gl_index_t* indices = speedometer_getIndices(speedometer, &iCount);
		gl_index_t attributes[] = {4};
		mesh_speedometer = gl_mesh_create((size2d_t){.height = vCount, .width = 4}, 3 * iCount, attributes, vertices, indices);
		if (!gl_mesh_check(&mesh_speedometer)) {
			speedometer_destroy(speedometer);
			fputs("Fail to create mesh for speedometer\n", stderr);
			goto label_exit;
		}

		texture_speedometer = gl_texture_create(gl_texformat_RGBA8, speedometer_size);
		if (!gl_texture_check(&texture_speedometer)) {
			fputs("Fail to create texture buffer for speedometer\n", stderr);
			goto label_exit;
		}
		
		gl_texture_update(gl_texformat_RGBA8, &texture_speedometer, speedometer_size, speedometer_getBitmap(speedometer));
		gl_synch barrier = gl_synchSet();
		if (gl_synchWait(barrier, FIVE_SECONDS_IN_NS) == gl_synch_timeout) {
			fputs("Speedometer GPU uploading fatal stall\n", stderr);
			gl_synchDelete(barrier);
			speedometer_destroy(speedometer);
			goto label_exit;
		}
		gl_synchDelete(barrier);

		speedometer_destroy(speedometer);
	}

	/* Create work buffer */ {
		framebuffer_edge[0] = gl_frameBuffer_create(RAW_COLORFORMAT, size2d);
		framebuffer_edge[1] = gl_frameBuffer_create(RAW_COLORFORMAT, size2d);
		if (!gl_frameBuffer_check(&framebuffer_edge[0]) || !gl_frameBuffer_check(&framebuffer_edge[1])) {
			fputs("Fail to create frame buffers to store edge\n", stderr);
			goto label_exit;
		}

		framebuffer_move[0] = gl_frameBuffer_create(RAW_COLORFORMAT, size2d);
		framebuffer_move[1] = gl_frameBuffer_create(RAW_COLORFORMAT, size2d);
		if (!gl_frameBuffer_check(&framebuffer_move[0]) || !gl_frameBuffer_check(&framebuffer_move[1])) {
			fputs("Fail to create frame buffers to store moving edge\n", stderr);
			goto label_exit;
		}

		framebuffer_speed = gl_frameBuffer_create(RAW_COLORFORMAT, size2d);
		if (!gl_frameBuffer_check(&framebuffer_speed)) {
			fputs("Fail to create frame buffers to store speed\n", stderr);
			goto label_exit;
		}
		
		framebuffer_stageA = gl_frameBuffer_create(RAW_COLORFORMAT, size2d);
		framebuffer_stageB = gl_frameBuffer_create(RAW_COLORFORMAT, size2d);
		if (!gl_frameBuffer_check(&framebuffer_stageA) || !gl_frameBuffer_check(&framebuffer_stageB)) {
			fputs("Fail to create work frame buffer A and/or B\n", stderr);
			goto label_exit;
		}
	}

	/* Create program: Roadmap check: Test roadmap geo data texture */ {
		const char* pName[] = {"size", "roadmap"};
		unsigned int pCount = sizeof(pName) / sizeof(pName[0]);
		gl_param pId[pCount];

		const char* bName[] = {};
		unsigned int bCount = sizeof(bName) / sizeof(bName[0]);
		gl_param bId[bCount];

		shader_roadmapCheck = gl_shader_load("shader/stdRect.vs.glsl", "shader/roadmapcheck.fs.glsl", 1, pName, pId, pCount, bName, bId, bCount);
		if (!gl_shader_check(&shader_roadmapCheck)) {
			fputs("Cannot load shader: roadmapcheck\n", stderr);
			goto label_exit;
		}

		gl_shader_use(&shader_roadmapCheck);
		gl_shader_setParam(pId[0], 2, gl_type_float, fsize);

		shader_roadmapCheck_paramRoadmap = pId[1];
	}

	/* Create program: Blit */ {
		const char* pName[] = {"size", "pStage"};
		unsigned int pCount = sizeof(pName) / sizeof(pName[0]);
		gl_param pId[pCount];

		const char* bName[] = {};
		unsigned int bCount = sizeof(bName) / sizeof(bName[0]);
		gl_param bId[bCount];

		shader_blit = gl_shader_load("shader/stdRect.vs.glsl", "shader/blit.fs.glsl", 1, pName, pId, pCount, bName, bId, bCount);
		if (!gl_shader_check(&shader_blit)) {
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
		if (!gl_shader_check(&shader_filter3)) {
			fputs("Cannot load shader: 3*3 filter\n", stderr);
			goto label_exit;
		}

		gl_shader_use(&shader_filter3);
		gl_shader_setParam(pId[0], 2, gl_type_float, fsize);

		shader_filter3_paramPStage = pId[1];
		shader_filter3_blockMask = bId[0];
	}

	/* Create program: Edge refine */ {
		const char* pName[] = {"size", "pStage", "threshold"};
		unsigned int pCount = sizeof(pName) / sizeof(pName[0]);
		gl_param pId[pCount];

		const char* bName[] = {};
		unsigned int bCount = sizeof(bName) / sizeof(bName[0]);
		gl_param bId[bCount];

		shader_edgerefine = gl_shader_load("shader/stdRect.vs.glsl", "shader/edgerefine.fs.glsl", 1, pName, pId, pCount, bName, bId, bCount);
		if (!gl_shader_check(&shader_edgerefine)) {
			fputs("Cannot load shader: edge refine\n", stderr);
			goto label_exit;
		}

		float threshold = 0.125f; //mat=0, exp=-3, Use >= and < when comparing, so the hardware only looks the exp

		gl_shader_use(&shader_edgerefine);
		gl_shader_setParam(pId[0], 2, gl_type_float, fsize);
		gl_shader_setParam(pId[2], 1, gl_type_float, &threshold);

		shader_edgerefine_paramPStage = pId[1];
	}

	/* Create program: Finding the difference between two texture (result = a - b) */ {
		const char* pName[] = {"size", "ta", "tb"};
		unsigned int pCount = sizeof(pName) / sizeof(pName[0]);
		gl_param pId[pCount];

		const char* bName[] = {};
		unsigned int bCount = sizeof(bName) / sizeof(bName[0]);
		gl_param bId[bCount];

		shader_different = gl_shader_load("shader/stdRect.vs.glsl", "shader/different.fs.glsl", 1, pName, pId, pCount, bName, bId, bCount);
		if (!gl_shader_check(&shader_different)) {
			fputs("Cannot load shader: find difference\n", stderr);
			goto label_exit;
		}

		gl_shader_use(&shader_different);
		gl_shader_setParam(pId[0], 2, gl_type_float, fsize);

		shader_different_paramA = pId[1];
		shader_different_paramB = pId[2];
	}

	/* Create program: Finding the displacement of (new edge in A) and (any edge in B) in road-domain */ {
		const char* pName[] = {"size", "maxDistance", "roadmap", "ta", "tb"};
		unsigned int pCount = sizeof(pName) / sizeof(pName[0]);
		gl_param pId[pCount];

		const char* bName[] = {};
		unsigned int bCount = sizeof(bName) / sizeof(bName[0]);
		gl_param bId[bCount];

		shader_distance = gl_shader_load("shader/stdRect.vs.glsl", "shader/distance.fs.glsl", 1, pName, pId, pCount, bName, bId, bCount);
		if (!gl_shader_check(&shader_distance)) {
			fputs("Cannot load shader: distance\n", stderr);
			goto label_exit;
		}

		gl_shader_use(&shader_distance);
		gl_shader_setParam(pId[0], 2, gl_type_float, fsize);
		gl_shader_setParam(pId[1], 1, gl_type_float, &distanceThreshold);

		shader_distance_paramRoadmap = pId[2];
		shader_distance_paramA = pId[3];
		shader_distance_paramB = pId[4];
	}

	/* Create program: Speedometer */ {
		const char* pName[] = {"size", "pStage", "glyphSize", "glyphTexture"};
		unsigned int pCount = sizeof(pName) / sizeof(pName[0]);
		gl_param pId[pCount];

		const char* bName[] = {};
		unsigned int bCount = sizeof(bName) / sizeof(bName[0]);
		gl_param bId[bCount];

		shader_speedometer = gl_shader_load("shader/speedometer.vs.glsl", "shader/speedometer.fs.glsl", 1, pName, pId, pCount, bName, bId, bCount);
		if (!gl_shader_check(&shader_distance)) {
			fputs("Cannot load shader: speedometer\n", stderr);
			goto label_exit;
		}

		gl_shader_use(&shader_speedometer);
		gl_shader_setParam(pId[0], 2, gl_type_float, fsize);
		gl_shader_setParam(pId[2], 2, gl_type_float, speedometer_sizeFloat);

		shader_speedometer_paramPStage = pId[1];
		shader_speedometer_paramGlyphTexture = pId[3];
	}

	/* Param - Kernel filter 3*3 masks */ {
		const float gussianMask[] = {
			1.0f/16,	2.0f/16,	1.0f/16,	0.0f, //vec4 alignment
			2.0f/16,	4.0f/16,	2.0f/16,	0.0f,
			1.0f/16,	2.0f/16,	1.0f/16,	0.0f
		};
		shader_ubo_gussianMask = gl_uniformBuffer_create(bindingPoint_ubo_gussianMask, sizeof(gussianMask));
		if (!gl_uniformBuffer_check(&shader_ubo_gussianMask)) {
			fputs("Cannot load shader UBO: Gussian mask\n", stderr);
			goto label_exit;
		}
		gl_uniformBuffer_update(&shader_ubo_gussianMask, 0, sizeof(gussianMask), gussianMask);

		const float edgeMask[] = {
			1.0f,	1.0f,	1.0f,	0.0f,
			1.0f,	-8.0f,	1.0f,	0.0f,
			1.0f,	1.0f,	1.0f,	0.0f
		};
		shader_ubo_edgeMask = gl_uniformBuffer_create(bindingPoint_ubo_edgeMask, sizeof(edgeMask));
		if (!gl_uniformBuffer_check(&shader_ubo_edgeMask)) {
			fputs("Cannot load shader UBO: Edge mask\n", stderr);
			goto label_exit;
		}
		gl_uniformBuffer_update(&shader_ubo_edgeMask, 0, sizeof(edgeMask), edgeMask);
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
		unsigned int rri = (unsigned int)(frameCount+0) & (unsigned int)1; //round-robin index - current
		unsigned int rrj = (unsigned int)(frameCount+1) & (unsigned int)1; //round-robin index - previous
		gl_drawStart(gl);
		
		/* Give next frame PBO to reader while using current PBO for rendering */ {
			#ifdef USE_PBO_UPLOAD
				gl_pixelBuffer_updateToTexture(RAW_COLORFORMAT, &cameraData[ (frameCount+0)&1 ], &texture_orginalBuffer, size2d); //Current frame PBO (index = +0) <-- Data already in GPU, this operation is fast
				rawDataPtr = gl_pixelBuffer_updateStart(&cameraData[ (frameCount+1)&1 ], size1d * 4); //Next frame PBO (index = +1)
			#else
				gl_texture_update(RAW_COLORFORMAT, &texture_orginalBuffer, size2d, rawData[ (frameCount+0)&1 ]);
				rawDataPtr = rawData[ (frameCount+1)&1 ];
			#endif
			sem_post(&sem_readerJobStart); //New GPU address ready, start to reader thread
		}

		/* Remove noise by using gussian blur mask */ {
			gl_shader* program = &shader_filter3;
			gl_fb* nextStage = &framebuffer_stageA;
			gl_param previousStageParam = shader_filter3_paramPStage;
			gl_tex* previousStage = &texture_orginalBuffer;
			gl_param param1 = shader_filter3_blockMask;
			unsigned int value1 = bindingPoint_ubo_gussianMask;
			gl_frameBuffer_bind(nextStage, size2d, 0);
			gl_shader_use(program);
			gl_uniformBuffer_bindShader(value1, program, param1);
			gl_texture_bind(previousStage, previousStageParam, 0);
			gl_mesh_draw(&mesh_region);
		}

		/* Apply edge detection mask to find edge */ {
			gl_shader* program = &shader_filter3;
			gl_fb* nextStage = &framebuffer_stageB;
			gl_param previousStageParam = shader_filter3_paramPStage;
			gl_tex* previousStage = &framebuffer_stageA.texture;
			gl_param param1 = shader_filter3_blockMask;
			unsigned int value1 = bindingPoint_ubo_edgeMask;
			gl_frameBuffer_bind(nextStage, size2d, 0);
			gl_shader_use(program);
			gl_uniformBuffer_bindShader(value1, program, param1);
			gl_texture_bind(previousStage, previousStageParam, 0);
			gl_mesh_draw(&mesh_region);
		}

		/* Refine the result of edge detection, remove noise, normalize */ {
			gl_shader* program = &shader_edgerefine;
			gl_fb* nextStage = &framebuffer_edge[rri];
			gl_param previousStageParam = shader_edgerefine_paramPStage;
			gl_tex* previousStage = &framebuffer_stageB.texture;
			gl_frameBuffer_bind(nextStage, size2d, 0);
			gl_shader_use(program);
			gl_texture_bind(previousStage, previousStageParam, 0);
			gl_mesh_draw(&mesh_region);
		}

		/* Compare current edge and previous edge to find moving edge */ {
			gl_shader* program = &shader_different;
			gl_fb* nextStage = &framebuffer_stageA;
			gl_param param1 = shader_different_paramA;
			gl_tex* value1 = &framebuffer_edge[rri].texture;
			gl_param param2 = shader_different_paramB;
			gl_tex* value2 = &framebuffer_edge[rrj].texture;
			gl_frameBuffer_bind(nextStage, size2d, 0);
			gl_shader_use(program);
			gl_texture_bind(value1, param1, 0);
			gl_texture_bind(value2, param2, 1);
			gl_mesh_draw(&mesh_region);
		}

		/* Refine the result of edge detection, remove noise, normalize */ {
			gl_shader* program = &shader_edgerefine;
			gl_fb* nextStage = &framebuffer_move[rri];
			gl_param previousStageParam = shader_edgerefine_paramPStage;
			gl_tex* previousStage = &framebuffer_stageA.texture;
			gl_frameBuffer_bind(nextStage, size2d, 0);
			gl_shader_use(program);
			gl_texture_bind(previousStage, previousStageParam, 0);
			gl_mesh_draw(&mesh_region);
		}

		/* Compare current moving edge and previous moving edge to find displacement of moving edge. New in A to nearest in B */ {
			gl_shader* program = &shader_distance;
			gl_fb* nextStage = &framebuffer_speed;
			gl_param param1 = shader_distance_paramA;
			gl_tex* value1 = &framebuffer_move[rri].texture;
			gl_param param2 = shader_distance_paramB;
			gl_tex* value2 = &framebuffer_move[rrj].texture;
			gl_param param3 = shader_distance_paramRoadmap;
			gl_tex* value3 = &texture_road;
			gl_frameBuffer_bind(nextStage, size2d, 0);
			gl_shader_use(program);
			gl_texture_bind(value1, param1, 0);
			gl_texture_bind(value2, param2, 1);
			gl_texture_bind(value3, param3, 2);
			gl_mesh_draw(&mesh_region);
		}

		/* Display the speedometer on screen */ {
			gl_shader* program = &shader_speedometer;
			gl_fb* nextStage = &framebuffer_stageA;
			gl_param previousStageParam = shader_speedometer_paramPStage;
			gl_tex* previousStage = &framebuffer_speed.texture;
			gl_param param1 = shader_speedometer_paramGlyphTexture;
			gl_tex* value1 = &texture_speedometer;
			gl_frameBuffer_bind(nextStage, size2d, 1);
			gl_shader_use(program);
			gl_texture_bind(previousStage, previousStageParam, 0);
			gl_texture_bind(value1, param1, 1);
			gl_mesh_draw(&mesh_speedometer);
		}

		/* Debug use ONLY: Check roadmap */ /*{
			gl_shader* program = &shader_roadmapCheck;
			gl_fb* nextStage = &framebuffer_stageA;
			gl_param previousStageParam = shader_roadmapCheck_paramRoadmap;
			gl_tex* previousStage = &texture_road;
			gl_frameBuffer_bind(nextStage, size2d, 0);
			gl_shader_use(program);
			gl_texture_bind(previousStage, previousStageParam, 0);
			gl_mesh_draw(&mesh_region);
		}*/

		gl_drawWindow(gl, &texture_orginalBuffer, &framebuffer_stageA.texture);
		gl_synch barrier = gl_synchSet();

		char title[60];
		sprintf(title, "Viewer - frame %u", frameCount);
		gl_drawEnd(gl, title);

		if (gl_synchWait(barrier, FIVE_SECONDS_IN_NS) == gl_synch_timeout) { //timeout = 5e9 ns = 5s
			fputs("Render loop fatal stall\n", stderr);
			goto label_exit;
		}
		gl_synchDelete(barrier);
		fputc('M', stdout); fflush(stdout);
		
		sem_wait(&sem_readerJobDone); //Wait reader thread finish uploading frame data
		#ifdef USE_PBO_UPLOAD
			gl_pixelBuffer_updateFinish();
		#endif

		endTime = nanotime();
		fprintf(stdout, " - Loop %u takes %lfms/frame.", frameCount, (endTime - startTime) / 1e6);
		fflush(stdout);
	
		frameCount++;
//		usleep(20e3);
	}



	/* Free all resources, house keeping */
	status = EXIT_SUCCESS;
label_exit:
	
	gl_unifromBuffer_delete(&shader_ubo_gussianMask);
	gl_unifromBuffer_delete(&shader_ubo_edgeMask);

	gl_shader_unload(&shader_speedometer);
	gl_shader_unload(&shader_distance);
	gl_shader_unload(&shader_different);
	gl_shader_unload(&shader_edgerefine);
	gl_shader_unload(&shader_filter3);
	gl_shader_unload(&shader_blit);
	gl_shader_unload(&shader_roadmapCheck);

	gl_frameBuffer_delete(&framebuffer_stageB);
	gl_frameBuffer_delete(&framebuffer_stageA);
	gl_frameBuffer_delete(&framebuffer_speed);
	gl_frameBuffer_delete(&framebuffer_move[1]);
	gl_frameBuffer_delete(&framebuffer_move[0]);
	gl_frameBuffer_delete(&framebuffer_edge[1]);
	gl_frameBuffer_delete(&framebuffer_edge[0]);

	gl_texture_delete(&texture_speedometer);

	gl_texture_delete(&texture_road);
	gl_mesh_delete(&mesh_region);

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