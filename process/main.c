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
#define MIXLEVEL 0.85f //0.88f

#define FRAME_DELAY (100 * 1000)
#define FRAME_DEBUGSKIPSECOND 20

#define FIFONAME "tmpframefifo.data"

#define INTERNAL_COLORFORMAT gl_texformat_RGBA32F //Must use RGBA16F or RGBA32F

#define FIVE_SECONDS_IN_NS 5000000000LLU //For gl sync timeout
#define GL_SYNCH_TIMEOUT FIVE_SECONDS_IN_NS

#define EDGE_THRESHOLD_CURRENT 1.1 //Minimum width of object (m)
#define EDGE_THRESHOLD_PREVIOUS 0.8 //For robust, current should be greater than previous

#define SPEEDOMETER_FILE "./textmap.data"
#define SPEEDOMETER_COLORFORMAT gl_texformat_RGBA8
#define SPEEDOMETER_CNT (size2d_t){.x = 27, .y = 30}

#define DEBUG_THREADSPEED
#ifdef DEBUG_THREADSPEED
volatile char debug_threadSpeed = ' '; //Which thread takes longer
#endif

//#define USE_PBO_UPLOAD

/* -- Reader thread ------------------------------------------------------------------------- */

volatile void volatile* rawDataPtr; //Video raw data goes here
sem_t sem_readerJobStart; //Fired by main thread: when pointer to pbo is ready, reader can begin to upload
sem_t sem_readerJobDone; //Fired by reader thread: when uploading is done, main thread can use
int sem_validFlag = 0; //For destroyer
enum sem_validFlag_Id {sem_validFlag_readerJobStart = 0b1, sem_validFlag_readerJobDone = 0b10};
struct th_reader_arg {
	size_t size; //Length of frame data in pixel
	const char* colorScheme; //Color scheme of the input raw data (numberOfChannel[1,3or4],orderOfChannelRGBA)
};
void* th_reader(void* arg) {
	struct th_reader_arg* this = arg;
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

	size_t frameSize = this->size;
	uint8_t colorRaw[4] = {0x00, 0x00, 0x00, 0xFF};
	int colorChCnt = this->colorScheme[0] - '0';
	fprintf(stdout, "[Reader] Frame size: %zu pixels. Number of color channel: %d\n", frameSize, colorChCnt);

	int colorChR = 0, colorChG = 1, colorChB = 2, colorChA = 3;
	if (this->colorScheme[1] != '\0')
		colorChR = this->colorScheme[1] - '0';
	if (this->colorScheme[2] != '\0')
		colorChG = this->colorScheme[2] - '0';
	if (this->colorScheme[3] != '\0')
		colorChB = this->colorScheme[3] - '0';
	if (this->colorScheme[4] != '\0')
		colorChA = this->colorScheme[4] - '0';
	
	if (colorChCnt == 1)
		fprintf(stdout, "[Reader] Color scheme: RGB = idx0, A = 0xFF\n");
	else if (colorChCnt == 3)
		fprintf(stdout, "[Reader] Color scheme: R = idx%d, G = idx%d, B = idx%d, A = 0xFF\n", colorChR, colorChG, colorChB);
	else
		fprintf(stdout, "[Reader] Color scheme: R = idx%d, G = idx%d, B = idx%d, A = idx%d\n", colorChR, colorChG, colorChB, colorChA);
	

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

		size_t iter = frameSize / bucketSize;
		uint8_t* dest = (void*)rawDataPtr;

		if (colorChCnt == 1) {
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
		} else if (colorChCnt == 3) {
			uint8_t rgb[bucketSize * 3];
			while (iter--) {
				uint8_t* p = &(rgb[0]);
				if (!fread(p, 3, bucketSize, fp)) {
					fileValid = 0;
					break;
				}
				while (p < &( rgb[bucketSize * 3] )) {
					*(dest++) = p[colorChR]; //R
					*(dest++) = p[colorChG]; //G
					*(dest++) = p[colorChB]; //B
					*(dest++) = 0xFF; //A
					p += 3;
				}
			}
		} else {
			if (colorChR == 0 && colorChG == 1 && colorChB == 2 && colorChA == 3) { //Video data scheme matched with GPU data scheme
				if (!fread(dest, 4, frameSize, fp)) {
					fileValid = 0;
					break;
				}
			} else {
				uint8_t rgba[bucketSize * 4];
				while (iter--) {
					uint8_t* p = &(rgba[0]);
					if (!fread(rgba, 4, bucketSize, fp)) {
						fileValid = 0;
						break;
					}
					while (p < &( rgba[bucketSize * 4] )) {
						*(dest++) = p[colorChR]; //R
						*(dest++) = p[colorChG]; //G
						*(dest++) = p[colorChB]; //B
						*(dest++) = p[colorChA]; //A
						p += 4;
					}
				}
			}
		}

		#ifdef DEBUG_THREADSPEED
			debug_threadSpeed = 'R';
		#endif
		sem_post(&sem_readerJobDone); //Uploading done, allow main thread to use it
	}

	fclose(fp);
	unlink(FIFONAME);
	fputs("[Reader] End of file or broken pipe! Please close the viewer window to terminate the program\n", stderr);

	while (1) { //Send dummy data to keep the main thread running
		sem_wait(&sem_readerJobStart); //Wait until main thread issue new GPU memory address for next frame
		memset((void*)rawDataPtr, 0, frameSize * 4);
		sem_post(&sem_readerJobDone); //Uploading done, allow main thread to use it
	}

	return NULL;
}

/* -- Main thread --------------------------------------------------------------------------- */

int main(int argc, char* argv[]) {
	int status = EXIT_FAILURE;
	unsigned int frameCnt = 0;
	
	size2d_t vSize; //Number of pixels in width and height in the input video
	const char* color; //Input video color scheme
	unsigned int fps; //Input video FPS
	const char* roadmapFile; //File dir - road-domain data (binary)

	/* Program argument check */ {
		if (argc != 6) {
			fputs(
				"Bad arg: Use 'this width height fps color roadmapFile'\n"
				"\twhere color = ncccc (n is number of channel input, cccc is the order of RGB[A])\n"
				"\t      roadmappFile = Directory to a binary coded file contains road-domain data\n"
			, stderr);
			return status;
		}
		unsigned int width = atoi(argv[1]), height = atoi(argv[2]);
		fps = atoi(argv[3]);
		color = argv[4];
		roadmapFile = argv[5];
		vSize = (size2d_t){.width = width, .height = height};
		fputs("Start...", stdout);
		fprintf(stdout, "\tWidth: %upx, Height: %upx, Total: %zusqpx\n", width, height, vSize.width * vSize.height);
		fprintf(stdout, "\tFPS: %u, Color: %s\n", fps, color);
		fprintf(stdout, "\tRoadmap: %s\n", roadmapFile);

		if (width & (unsigned int)0b11 || width < 320 || width > 4056) {
			fputs("Bad width: Width must be multiple of 4, 320 <= width <= 4056\n", stderr);
			return status;
		}
		if (height & (unsigned int)0b11 || height < 240 || height > 3040) {
			fputs("Bad height: Height must be multiple of 4, 240 <= height <= 3040\n", stderr);
			return status;
		}
		if (color[0] != '1' && color[0] != '3' && color[0] != '4') {
			fputs("Bad color: Color channel must be 1, 3 or 4\n", stderr);
			return status;
		}
		if (strnlen(color, 10) != color[0] - '0' + 1) {
			fputs("Bad color: Color scheme and channel count mismatched\n", stderr);
			return status;
		}
		for (int i = 1; i < strlen(color); i++) {
			if (color[i] < '0' || color[i] >= color[0]) {
				fprintf(stderr, "Bad color: Each color scheme must be in the range of [0, channel-1 (%d)]\n", color[0] - '0' - 1);
				return status;
			}
		}
	}

	/* Program variables declaration */

	//PBO or memory space for orginal video raw data uploading, and textures to store orginal video data
	#ifdef USE_PBO_UPLOAD
		gl_pbo pbo[2] = {GL_INIT_DEFAULT_PBO, GL_INIT_DEFAULT_PBO};
	#else
		void* rawData[2] = {NULL, NULL};
	#endif
	gl_tex texture_orginalBuffer = GL_INIT_DEFAULT_TEX;

	//Reader thread used to read orginal video raw data from external source
	pthread_t th_reader_id;
	struct th_reader_arg th_reader_arg;
	int th_reader_idValid = 0;

	//Roadinfo, a mesh to store focus region, and texture to store road-domain data
	roadmap_header roadinfo;
	gl_mesh mesh_persp = GL_INIT_DEFAULT_MESH;
	gl_mesh mesh_ortho = GL_INIT_DEFAULT_MESH;
	gl_tex texture_roadmap1 = GL_INIT_DEFAULT_TEX;
	gl_tex texture_roadmap2 = GL_INIT_DEFAULT_TEX;

	//To display human readable text on screen
	const char* speedometer_filename = SPEEDOMETER_FILE;
	size2d_t speedometer_cnt = SPEEDOMETER_CNT;
	gl_mesh mesh_speedsample = GL_INIT_DEFAULT_MESH;
	gl_mesh mesh_speedometer = GL_INIT_DEFAULT_MESH;
	gl_tex texture_speedometer = GL_INIT_DEFAULT_TEX;

	//Work stages: To save intermediate results during the process and temp buffer
	gl_fb framebuffer_raw[2] = {GL_INIT_DEFAULT_FB, GL_INIT_DEFAULT_FB}; //Raw video data of current and previous frame (with minor pre-process)
	gl_fb framebuffer_edge[2] = {GL_INIT_DEFAULT_FB, GL_INIT_DEFAULT_FB}; //Edge of current and previous frame
	gl_fb framebuffer_speed = GL_INIT_DEFAULT_FB; //Displacement of two moving edge (speed of moving edge)
	gl_fb framebuffer_display = GL_INIT_DEFAULT_FB; //Display human-readable text
	gl_fb framebuffer_stageA = GL_INIT_DEFAULT_FB;
	gl_fb framebuffer_stageB = GL_INIT_DEFAULT_FB;

	//Program[Debug] - Roadmap check: Roadmap match with video
	gl_shader shader_roadmapCheck = GL_INIT_DEFAULT_SHADER;
	gl_param shader_roadmapCheck_paramRoadmapT1;
	gl_param shader_roadmapCheck_paramRoadmapT2;
	gl_param shader_roadmapCheck_paramCfgI1;
	gl_param shader_roadmapCheck_paramCfgF1;

	//Program - Blur
	gl_shader shader_blur = GL_INIT_DEFAULT_SHADER;
	gl_param shader_blur_paramSrc;

	//Program - Changing sensor
	gl_shader shader_changingSensor = GL_INIT_DEFAULT_SHADER;
	gl_param shader_changingSensor_paramCurrent;
	gl_param shader_changingSensor_paramPrevious;

	//Program - Edge filter
	gl_shader shader_edgeFilter = GL_INIT_DEFAULT_SHADER;
	gl_param shader_edgeFilter_paramSrc;

	//Program - Edge refine
	gl_shader shader_edgeRefine = GL_INIT_DEFAULT_SHADER;
	gl_param shader_edgeRefine_paramSrc;

	//Program - Project P/O
	gl_shader shader_projectPO = GL_INIT_DEFAULT_SHADER;
	gl_param shader_projectPO_paramSrc;
	gl_param shader_projectPO_paramRoadmapT2;
	gl_param shader_projectPO_paramMode;

	//Program - Edge determine
	gl_shader shader_edgeDetermine = GL_INIT_DEFAULT_SHADER;
	gl_param shader_edgeDetermine_paramCurrent;
	gl_param shader_edgeDetermine_paramPrevious;

	//Program - Distance measure
	gl_shader shader_distanceMeasure = GL_INIT_DEFAULT_SHADER;
	gl_param shader_distanceMeasure_paramEdgemap;
	gl_param shader_distanceMeasure_paramRoadmapT1;
	gl_param shader_distanceMeasure_paramRoadmapT2;

	//Program - Speed sample
	gl_shader shader_speedSample = GL_INIT_DEFAULT_SHADER;
	gl_param shader_speedSample_paramSpeedmap;

	//Program - Speedometer
	gl_shader shader_speedometer = GL_INIT_DEFAULT_SHADER;
	gl_param shader_speedometer_paramSpeedmap;
	gl_param shader_speedometer_paramGlyphmap;

	/* Init OpenGL and viewer window */ {
		if (!gl_init(vSize, WINDOW_RATIO, MIXLEVEL)) {
			fputs("Cannot init OpenGL\n", stderr);
			goto label_exit;
		}

		#ifdef VERBOSE
			fputs("OpenGL driver and hardware info:\n", stdout);
			int i[1];
			float f[0];
			
			fprintf(stdout, "\t- Vendor:                   %s\n", (const char*)gl_getInfo(gl_info_s_vendor, NULL));
			fprintf(stdout, "\t- Renderer:                 %s\n", (const char*)gl_getInfo(gl_info_s_renderer, NULL));
			fprintf(stdout, "\t- Version:                  %s\n", (const char*)gl_getInfo(gl_info_s_version, NULL));
			fprintf(stdout, "\t- Shader language version:  %s\n", (const char*)gl_getInfo(gl_info_s_shadingLanguageVersion, NULL));

			gl_getInfo(gl_info_i1_maxTextureSize, i);
			fprintf(stdout, "\t- Max texture size:         %d\n", i[0]);
			gl_getInfo(gl_info_i1_max3dTextureSize, i);
			fprintf(stdout, "\t- Max 3D texture size:      %d\n", i[0]);
			gl_getInfo(gl_info_i1_maxArrayTextureLayers, i);
			fprintf(stdout, "\t- Max texture layers:       %d\n", i[0]);
			gl_getInfo(gl_info_i1_maxTextureImageUnits, i);
			fprintf(stdout, "\t- Max texture Units:        %d\n", i[0]);
			gl_getInfo(gl_info_i1_maxVertexTextureImageUnits, i);
			fprintf(stdout, "\t- Max Vertex texture units: %d\n", i[0]);

			fflush(stdout);
		#endif
	}

	/* Use a texture to store raw frame data & Start reader thread */ {
		#ifdef USE_PBO_UPLOAD
			pbo[0] = gl_pixelBuffer_create(vSize.width * vSize.height * 4); //Always use RGBA8 (good performance)
			pbo[1] = gl_pixelBuffer_create(vSize.width * vSize.height * 4);
			if (!gl_pixelBuffer_check(&(pbo[0])) || !gl_pixelBuffer_check(&(pbo[1]))) {
				fputs("Fail to create pixel buffers for orginal frame data uploading\n", stderr);
				goto label_exit;
			}
		#else
			rawData[0] = malloc(vSize.width * vSize.height * 4); //Always use RGBA8 (good performance)
			rawData[1] = malloc(vSize.width * vSize.height * 4);
			if (!rawData[0] || !rawData[1]) {
				fputs("Fail to create memory buffers for orginal frame data loading\n", stderr);
				goto label_exit;
			}
		#endif

		texture_orginalBuffer = gl_texture_create(gl_texformat_RGBA8, vSize);
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
		th_reader_arg = (struct th_reader_arg){.size = vSize.width * vSize.height, .colorScheme = color};
		int err = pthread_create(&th_reader_id, &attr, th_reader, &th_reader_arg);
		if (err) {
			fprintf(stderr, "Fail to init reader thread (%d)\n", err);
			goto label_exit;
		}
		th_reader_idValid = 1;
	}

	/* Roadmap: mesh for focus region, road info, textures for road data */ {
		Roadmap roadmap = roadmap_init(roadmapFile, vSize);
		if (!roadmap) {
			fputs("Cannot load roadmap\n", stderr);
			goto label_exit;
		}

		roadinfo = roadmap_getHeader(roadmap);
		fprintf(stdout, "Roadmap threshold: %f/frame\n", roadinfo.searchThreshold);

		/* Focus region */ {
			gl_index_t attributes[] = {2};
			size_t vCnt;
			gl_vertex_t* vertices = roadmap_getRoadPoints(roadmap, &vCnt);

			float inLeft = 0.0f, inRight = 1.0f, inTop = 0.0f, inBottom = 1.0f;
			for (unsigned int i = 0; i < vCnt; i++) {
				inLeft = fmaxf(inLeft, vertices[i*2+0]);
				inRight = fminf(inRight, vertices[i*2+0]);
				inTop = fmaxf(inTop, vertices[i*2+1]);
				inBottom = fminf(inBottom, vertices[i*2+1]);
			}
			gl_vertex_t vPersp[vCnt+2][2];
			for (unsigned int i = 0; i < vCnt; i++) {
				vPersp[i+1][0] = vertices[i*2+0];
				vPersp[i+1][1] = vertices[i*2+1];
			}
			vPersp[0][0] = (inLeft + inRight) * 0.5f;
			vPersp[0][1] = (inTop + inBottom) * 0.5f;
			vPersp[vCnt+1][0] = vertices[0];
			vPersp[vCnt+1][1] = vertices[1];

			float outLeft = 0.0f, outRight = 1.0f, outTop = 0.0f, outBottom = 1.0f;
			for (unsigned int i = 0; i < vCnt; i++) {
				outLeft = fminf(outLeft, vertices[i*2+0]);
				outRight = fmaxf(outRight, vertices[i*2+0]);
				outTop = fminf(outTop, vertices[i*2+1]);
				outBottom = fmaxf(outBottom, vertices[i*2+1]);
			}
			gl_vertex_t vOthor[4][2] = { {outLeft, outTop}, {outRight, outTop}, {outRight, outBottom}, {outLeft, outBottom} };

			mesh_persp = gl_mesh_create((size2d_t){.height=vCnt+2, .width=2}, 0, attributes, (gl_vertex_t*)vPersp, NULL, gl_meshmode_triangleFan);
			mesh_ortho = gl_mesh_create((size2d_t){.height=4, .width=2}, 0, attributes, (gl_vertex_t*)vOthor, NULL, gl_meshmode_triangleFan);
			if ( !gl_mesh_check(&mesh_persp) || !gl_mesh_check(&mesh_ortho) ) {
				roadmap_destroy(roadmap);
				fputs("Fail to create mesh for roadmap - focus region\n", stderr);
				goto label_exit;
			}
		}
		
		/* Road data */ {
			texture_roadmap1 = gl_texture_create(gl_texformat_RGBA32F, vSize);
			texture_roadmap2 = gl_texture_create(gl_texformat_RGBA32I, vSize);
			if (!gl_texture_check(&texture_roadmap1) || !gl_texture_check(&texture_roadmap2)) {
				roadmap_destroy(roadmap);
				fputs("Fail to create texture buffer for roadmap - road-domain data storage\n", stderr);
				goto label_exit;
			}

			gl_texture_update(gl_texformat_RGBA32F, &texture_roadmap1, vSize, roadmap_getT1(roadmap));
			gl_texture_update(gl_texformat_RGBA32I, &texture_roadmap2, vSize, roadmap_getT2(roadmap));
		}

		gl_synch barrier = gl_synchSet();
		if (gl_synchWait(barrier, GL_SYNCH_TIMEOUT) == gl_synch_timeout) {
			fputs("Roadmap GPU uploading fatal stall\n", stderr);
			gl_synchDelete(barrier);
			roadmap_destroy(roadmap);
			goto label_exit;
		}
		gl_synchDelete(barrier);

		roadmap_destroy(roadmap); //Free roadmap memory after data uploading finished
	}

	/* Speedometer: speedometer sample region, display location and glphy texture */ {
		Speedometer speedometer = speedometer_init(speedometer_filename);
		if (!speedometer) {
			fputs("Cannot load speedometer\n", stderr);
			goto label_exit;
		}
		if (!speedometer_convert(speedometer)) {
			fputs("Cannot convert speedometer layout\n", stderr);
			goto label_exit;
		}

		size2d_t glyphSize;
		uint32_t* glyph = speedometer_getGlyph(speedometer, &glyphSize);
		texture_speedometer = gl_textureArray_create(SPEEDOMETER_COLORFORMAT, (size3d_t){.width=glyphSize.width, .height=glyphSize.height, .depth=256});
		if (!gl_texture_check(&texture_speedometer)) {
			fputs("Fail to create texture buffer for speedometer\n", stderr);
			goto label_exit;
		}
		for (int i = 0; i < 256; i++) {
			uint32_t* ptr = &glyph[i * glyphSize.width * glyphSize.height];
			gl_textureArray_update(SPEEDOMETER_COLORFORMAT, &texture_speedometer, (size3d_t){.x=glyphSize.width, .y=glyphSize.height, .z=i}, ptr);
		}

		float xStep = 1.0f / speedometer_cnt.width, yStep = 1.0f / speedometer_cnt.height;
		gl_index_t attribute[] = {4};
		gl_vertex_t vSample[speedometer_cnt.height][speedometer_cnt.width][4];
		for (int y = 0; y < speedometer_cnt.height; y++) {
			for (int x = 0; x < speedometer_cnt.width; x++) {
				vSample[y][x][0] = xStep * x;
				vSample[y][x][1] = yStep * y;
				vSample[y][x][2] = xStep * x + xStep;
				vSample[y][x][3] = yStep * y + yStep;
			}
		}
		mesh_speedsample = gl_mesh_create((size2d_t){.height = speedometer_cnt.height * speedometer_cnt.width, .width = 4}, 0, attribute, (gl_vertex_t*)vSample, NULL, gl_meshmode_points);
		if (!gl_mesh_check(&mesh_speedsample)) {
			speedometer_destroy(speedometer);
			fputs("Fail to create mesh for speedometer\n", stderr);
			goto label_exit;
		}

/*		vec4 (* vMeter)[speedometer_cnt.width][6] = malloc(speedometer_cnt.height * speedometer_cnt.width * 6 * sizeof(vec4));
		if (!vMeter) {
			speedometer_destroy(speedometer);
			fputs("Fail to allocate memory for speedometer vertices\n", stderr);
			goto label_exit;
		}*/
		vec4 vMeter[speedometer_cnt.height][speedometer_cnt.width][6];
		for (int y = 0; y <= speedometer_cnt.height; y++) {
			for (int x = 0; x <= speedometer_cnt.width; x++) {
				float left = xStep * x, right = xStep * x + xStep;
				float top = yStep * y, bottom = yStep * y + yStep;
				float center = (left + right) / 2, middle = (top + bottom) / 2;
				vMeter[y][x][0] = (vec4){	.x=left,	.y=top,		.z=center,	.w=middle	};
				vMeter[y][x][1] = (vec4){	.x=right,	.y=top,		.z=center,	.w=middle	};
				vMeter[y][x][2] = (vec4){	.x=left,	.y=bottom,	.z=center,	.w=middle	};
				vMeter[y][x][3] = (vec4){	.x=right,	.y=top,		.z=center,	.w=middle	};
				vMeter[y][x][4] = (vec4){	.x=left,	.y=bottom,	.z=center,	.w=middle	};
				vMeter[y][x][5] = (vec4){	.x=right,	.y=bottom,	.z=center,	.w=middle	};
			}
		}
		mesh_speedometer = gl_mesh_create((size2d_t){.height = speedometer_cnt.height * speedometer_cnt.width * 6, .width = 4}, 0, attribute, (gl_vertex_t*)vMeter, NULL, gl_meshmode_triangles);
		if (!gl_mesh_check(&mesh_speedometer)) {
			speedometer_destroy(speedometer);
			fputs("Fail to create mesh for speedometer\n", stderr);
			goto label_exit;
		}

		gl_synch barrier = gl_synchSet();
		if (gl_synchWait(barrier, GL_SYNCH_TIMEOUT) == gl_synch_timeout) {
			fputs("Speedometer GPU uploading fatal stall\n", stderr);
			gl_synchDelete(barrier);
			speedometer_destroy(speedometer);
			goto label_exit;
		}
		gl_synchDelete(barrier);

		speedometer_destroy(speedometer);
	}

	/* Create work buffer */ {
		framebuffer_raw[0] = gl_frameBuffer_create(INTERNAL_COLORFORMAT, vSize);
		framebuffer_raw[1] = gl_frameBuffer_create(INTERNAL_COLORFORMAT, vSize);
		if (!gl_frameBuffer_check(&framebuffer_raw[0]) || !gl_frameBuffer_check(&framebuffer_raw[1])) {
			fputs("Fail to create frame buffers to store raw video data\n", stderr);
			goto label_exit;
		}

		framebuffer_edge[0] = gl_frameBuffer_create(INTERNAL_COLORFORMAT, vSize);
		framebuffer_edge[1] = gl_frameBuffer_create(INTERNAL_COLORFORMAT, vSize);
		if (!gl_frameBuffer_check(&framebuffer_edge[0]) || !gl_frameBuffer_check(&framebuffer_edge[1])) {
			fputs("Fail to create frame buffers to store edge\n", stderr);
			goto label_exit;
		}

		framebuffer_speed = gl_frameBuffer_create(INTERNAL_COLORFORMAT, vSize);
		if (!gl_frameBuffer_check(&framebuffer_speed)) {
			fputs("Fail to create frame buffers to store speed\n", stderr);
			goto label_exit;
		}

		framebuffer_display = gl_frameBuffer_create(gl_texformat_RGBA8, vSize);
		if (!gl_frameBuffer_check(&framebuffer_display)) {
			fputs("Fail to create frame buffers to store human-readable text\n", stderr);
			goto label_exit;
		}
		
		framebuffer_stageA = gl_frameBuffer_create(INTERNAL_COLORFORMAT, vSize);
		framebuffer_stageB = gl_frameBuffer_create(INTERNAL_COLORFORMAT, vSize);
		if (!gl_frameBuffer_check(&framebuffer_stageA) || !gl_frameBuffer_check(&framebuffer_stageB)) {
			fputs("Fail to create work frame buffer A and/or B\n", stderr);
			goto label_exit;
		}
	}

	#define shader_setParam(id, type, data) gl_shader_setParam(id, arrayLength(data), type, data)
	const char* glversionStr = "#version 310 es\n";
	const char* glPrecisionStr = "precision mediump float;\n";

	/* Create program: Roadmap check */ {
		gl_shaderSrc vs[] = {
			{.isFile = 0, .src = glversionStr},
			{.isFile = 0, .src = glPrecisionStr},
			{.isFile = 1, .src = "shader/focusRegion.vs.glsl"}
		};
		gl_shaderSrc fs[] = {
			{.isFile = 0, .src = glversionStr},
			{.isFile = 0, .src = glPrecisionStr},
			{.isFile = 1, .src = "shader/roadmapCheck.fs.glsl"}
		};
		gl_shaderArg args[] = {
			{.isUBO = 0, .name = "roadmapT1"},
			{.isUBO = 0, .name = "roadmapT2"},
			{.isUBO = 0, .name = "cfgI1"},
			{.isUBO = 0, .name = "cfgF1"}
		};
		shader_roadmapCheck = gl_shader_create((ivec4){arrayLength(vs), arrayLength(fs), 0, arrayLength(args)}, vs, fs, NULL, args);
		if (!shader_roadmapCheck) {
			fputs("Cannot load shader: Roadmap check\n", stderr);
			goto label_exit;
		}
		shader_roadmapCheck_paramRoadmapT1 = args[0].id;
		shader_roadmapCheck_paramRoadmapT2 = args[1].id;
		shader_roadmapCheck_paramCfgI1 = args[2].id;
		shader_roadmapCheck_paramCfgF1 = args[3].id;

		gl_shader_use(&shader_roadmapCheck);
		ivec4 cfgI1 = {5, 0, 0, 0};
		gl_shader_setParam(shader_roadmapCheck_paramCfgI1, 4, gl_type_int, &cfgI1);
		vec4 cfgF1 = {1.0, 1.0, 1.0, 1.0};
		gl_shader_setParam(shader_roadmapCheck_paramCfgF1, 4, gl_type_float, &cfgF1);
	}

	/* Create program: Blur */ {
		gl_shaderSrc vs[] = {
			{.isFile = 0, .src = glversionStr},
			{.isFile = 0, .src = glPrecisionStr},
			{.isFile = 1, .src = "shader/focusRegion.vs.glsl"}
		};
		gl_shaderSrc fs[] = {
			{.isFile = 0, .src = glversionStr},
			{.isFile = 0, .src = glPrecisionStr},
			{.isFile = 1, .src = "shader/blur.fs.glsl"}
		};
		gl_shaderArg args[] = {
			{.isUBO = 0, .name = "src"}
		};
		shader_blur = gl_shader_create((ivec4){arrayLength(vs), arrayLength(fs), 0, arrayLength(args)}, vs, fs, NULL, args);
		if (!shader_blur) {
			fputs("Cannot load shader: Blur\n", stderr);
			goto label_exit;
		}
		shader_blur_paramSrc = args[0].id;
	}

	/* Create program: Changing sensor */ {
		gl_shaderSrc vs[] = {
			{.isFile = 0, .src = glversionStr},
			{.isFile = 0, .src = glPrecisionStr},
			{.isFile = 1, .src = "shader/focusRegion.vs.glsl"}
		};
		gl_shaderSrc fs[] = {
			{.isFile = 0, .src = glversionStr},
			{.isFile = 0, .src = glPrecisionStr},
			{.isFile = 1, .src = "shader/changingSensor.fs.glsl"}
		};
		gl_shaderArg args[] = {
			{.isUBO = 0, .name = "current"},
			{.isUBO = 0, .name = "previous"}
		};
		shader_changingSensor = gl_shader_create((ivec4){arrayLength(vs), arrayLength(fs), 0, arrayLength(args)}, vs, fs, NULL, args);
		if (!shader_changingSensor) {
			fputs("Cannot load shader: Changing sensor\n", stderr);
			goto label_exit;
		}
		shader_changingSensor_paramCurrent = args[0].id;
		shader_changingSensor_paramPrevious = args[1].id;
	}

	/* Create program: Edge filter */ {
		gl_shaderSrc vs[] = {
			{.isFile = 0, .src = glversionStr},
			{.isFile = 0, .src = glPrecisionStr},
			{.isFile = 1, .src = "shader/focusRegion.vs.glsl"}
		};
		gl_shaderSrc fs[] = {
			{.isFile = 0, .src = glversionStr},
			{.isFile = 0, .src = glPrecisionStr},
			{.isFile = 1, .src = "shader/edgeFilter.fs.glsl"}
		};
		gl_shaderArg args[] = {
			{.isUBO = 0, .name = "src"}
		};
		shader_edgeFilter = gl_shader_create((ivec4){arrayLength(vs), arrayLength(fs), 0, arrayLength(args)}, vs, fs, NULL, args);
		if (!shader_edgeFilter) {
			fputs("Cannot load shader: Edge filter\n", stderr);
			goto label_exit;
		}
		shader_edgeFilter_paramSrc = args[0].id;
	}

	/* Create program: Edge refine */ {
		gl_shaderSrc vs[] = {
			{.isFile = 0, .src = glversionStr},
			{.isFile = 0, .src = glPrecisionStr},
			{.isFile = 1, .src = "shader/focusRegion.vs.glsl"}
		};
		gl_shaderSrc fs[] = {
			{.isFile = 0, .src = glversionStr},
			{.isFile = 0, .src = glPrecisionStr},
			{.isFile = 1, .src = "shader/edgeRefine.fs.glsl"}
		};
		gl_shaderArg args[] = {
			{.isUBO = 0, .name = "src"}
		};
		shader_edgeRefine = gl_shader_create((ivec4){arrayLength(vs), arrayLength(fs), 0, arrayLength(args)}, vs, fs, NULL, args);
		if (!shader_edgeRefine) {
			fputs("Cannot load shader: Edge refine\n", stderr);
			goto label_exit;
		}
		shader_edgeRefine_paramSrc = args[0].id;
	}

	/* Create program: Project P/O*/ {
		gl_shaderSrc vs[] = {
			{.isFile = 0, .src = glversionStr},
			{.isFile = 0, .src = glPrecisionStr},
			{.isFile = 1, .src = "shader/focusRegion.vs.glsl"}
		};
		gl_shaderSrc fs[] = {
			{.isFile = 0, .src = glversionStr},
			{.isFile = 0, .src = glPrecisionStr},
			{.isFile = 1, .src = "shader/projectPO.fs.glsl"}
		};
		gl_shaderArg args[] = {
			{.isUBO = 0, .name = "src"},
			{.isUBO = 0, .name = "roadmapT2"},
			{.isUBO = 0, .name = "mode"}
		};
		shader_projectPO = gl_shader_create((ivec4){arrayLength(vs), arrayLength(fs), 0, arrayLength(args)}, vs, fs, NULL, args);
		if (!shader_projectPO) {
			fputs("Cannot load shader: Project P/O\n", stderr);
			goto label_exit;
		}
		shader_projectPO_paramSrc = args[0].id;
		shader_projectPO_paramRoadmapT2 = args[1].id;
		shader_projectPO_paramMode = args[2].id;
	}

	/* Create program: Edge determine */ {
		gl_shaderSrc vs[] = {
			{.isFile = 0, .src = glversionStr},
			{.isFile = 0, .src = glPrecisionStr},
			{.isFile = 1, .src = "shader/focusRegion.vs.glsl"}
		};
		gl_shaderSrc fs[] = {
			{.isFile = 0, .src = glversionStr},
			{.isFile = 0, .src = glPrecisionStr},
			{.isFile = 1, .src = "shader/edgeDetermine.fs.glsl"}
		};
		gl_shaderArg args[] = {
			{.isUBO = 0, .name = "current"},
			{.isUBO = 0, .name = "previous"},
			{.isUBO = 0, .name = "threshold"}
		};
		shader_edgeDetermine = gl_shader_create((ivec4){arrayLength(vs), arrayLength(fs), 0, arrayLength(args)}, vs, fs, NULL, args);
		if (!shader_edgeDetermine) {
			fputs("Cannot load shader: Edge determine\n", stderr);
			goto label_exit;
		}
		shader_edgeDetermine_paramCurrent = args[0].id;
		shader_edgeDetermine_paramPrevious = args[1].id;

		gl_shader_use(&shader_edgeDetermine);
		ivec4 threshold = {
			EDGE_THRESHOLD_CURRENT / roadinfo.orthoPixelWidth,
			EDGE_THRESHOLD_PREVIOUS / roadinfo.orthoPixelWidth,
			0,
			0
		};
		gl_shader_setParam(args[2].id, 4, gl_type_int, &threshold);
	}
	
	/* Create program: Distance measure */ {
		gl_shaderSrc vs[] = {
			{.isFile = 0, .src = glversionStr},
			{.isFile = 0, .src = glPrecisionStr},
			{.isFile = 1, .src = "shader/focusRegion.vs.glsl"}
		};
		gl_shaderSrc fs[] = {
			{.isFile = 0, .src = glversionStr},
			{.isFile = 0, .src = glPrecisionStr},
			{.isFile = 1, .src = "shader/distanceMeasure.fs.glsl"}
		};
		gl_shaderArg args[] = {
			{.isUBO = 0, .name = "edgemap"},
			{.isUBO = 0, .name = "roadmapT1"},
			{.isUBO = 0, .name = "roadmapT2"},
			{.isUBO = 0, .name = "bias"}
		};
		shader_distanceMeasure = gl_shader_create((ivec4){arrayLength(vs), arrayLength(fs), 0, arrayLength(args)}, vs, fs, NULL, args);
		if (!shader_distanceMeasure) {
			fputs("Cannot load shader: Distance measure\n", stderr);
			goto label_exit;
		}
		shader_distanceMeasure_paramEdgemap = args[0].id;
		shader_distanceMeasure_paramRoadmapT1 = args[1].id;
		shader_distanceMeasure_paramRoadmapT2 = args[2].id;
		
		gl_shader_use(&shader_distanceMeasure);
		vec4 bias = {
			0.0,		//x = bias - offset
			fps * 3.6,	//y = bias - first orade: m/frame --> m/s --> km/h
			0.0,		//Currently not used
			0.0		//Currently not used
		};

		gl_shader_setParam(args[3].id, 4, gl_type_float, &bias);
	}

	/* Create program: Speed sample */ { //This program can be combined with speedometer if geometry shader is supported
		gl_shaderSrc vs[] = {
			{.isFile = 0, .src = glversionStr},
			{.isFile = 0, .src = glPrecisionStr},
			{.isFile = 0, .src = "const int speedPoolSize = 8;\nconst float speedSensitive = 10.0;\nconst int speedDisplayCutoff = 4;"},
			{.isFile = 1, .src = "shader/speedSample.vs.glsl"}
		};
		gl_shaderSrc fs[] = {
			{.isFile = 0, .src = glversionStr},
			{.isFile = 0, .src = glPrecisionStr},
			{.isFile = 1, .src = "shader/speedSample.fs.glsl"}
		};
		gl_shaderArg args[] = {
			{.isUBO = 0, .name = "speedmap"}
		};
		shader_speedSample = gl_shader_create((ivec4){arrayLength(vs), arrayLength(fs), 0, arrayLength(args)}, vs, fs, NULL, args);
		if (!shader_speedSample) {
			fputs("Cannot load shader: Speed sample\n", stderr);
			goto label_exit;
		}
		shader_speedSample_paramSpeedmap = args[0].id;
	}

	/* Create program: Speedometer */ {
		gl_shaderSrc vs[] = {
			{.isFile = 0, .src = glversionStr},
			{.isFile = 0, .src = glPrecisionStr},
			{.isFile = 1, .src = "shader/speedometer.vs.glsl"}
		};
		gl_shaderSrc fs[] = {
			{.isFile = 0, .src = glversionStr},
			{.isFile = 0, .src = glPrecisionStr},
			{.isFile = 1, .src = "shader/speedometer.fs.glsl"}
		};
		gl_shaderArg args[] = {
			{.isUBO = 0, .name = "speedmap"},
			{.isUBO = 0, .name = "glyphmap"}
		};
		shader_speedometer = gl_shader_create((ivec4){arrayLength(vs), arrayLength(fs), 0, arrayLength(args)}, vs, fs, NULL, args);
		if (!shader_speedometer) {
			fputs("Cannot load shader: Speedometer\n", stderr);
			goto label_exit;
		}
		shader_speedometer_paramSpeedmap = args[0].id;
		shader_speedometer_paramGlyphmap = args[1].id;
	}

	void ISR_SIGINT() {
		gl_close(1);
	}
	signal(SIGINT, ISR_SIGINT);

	/* Main process loop here */
	uint64_t startTime, endTime;
	while(!gl_close(-1)) {
		fputc('\r', stdout); //Clear output

		startTime = nanotime(); //Benchmark

		unsigned int rri = (unsigned int)(frameCnt+0) & (unsigned int)1; //round-robin index - current
		unsigned int rrj = (unsigned int)(frameCnt+1) & (unsigned int)1; //round-robin index - previous
		
		size2d_t cursorPos;
		gl_drawStart(&cursorPos);
		
		/* Give next frame PBO to reader while using current PBO for rendering */ {
			#ifdef USE_PBO_UPLOAD
				gl_pixelBuffer_updateToTexture(gl_texformat_RGBA8, &pbo[ (frameCnt+0)&1 ], &texture_orginalBuffer, vSize); //Current frame PBO (index = +0) <-- Data already in GPU, this operation is fast
				rawDataPtr = gl_pixelBuffer_updateStart(&pbo[ (frameCnt+1)&1 ], vSize.width * vSize.height * 4); //Next frame PBO (index = +1)
			#else
				gl_texture_update(gl_texformat_RGBA8, &texture_orginalBuffer, vSize, rawData[ (frameCnt+0)&1 ]);
				rawDataPtr = rawData[ (frameCnt+1)&1 ];
			#endif
			sem_post(&sem_readerJobStart); //New GPU address ready, start to reader thread
		}

		/* Debug use ONLY: Check roadmap */ /*{
			gl_shader_use(&shader_roadmapCheck);
			gl_texture_bind(&texture_roadmap1, shader_roadmapCheck_paramRoadmapT1, 0);
			gl_texture_bind(&texture_roadmap2, shader_roadmapCheck_paramRoadmapT2, 1);
			gl_frameBuffer_bind(&framebuffer_stageA, vSize, 1);
			gl_mesh_draw(NULL);
		}*/

		/* Debug use ONLY: Check orthographic transform */ /*{
			ivec4 mode = {1, 0, 0, 0};
			gl_shader_use(&shader_projectPO);
			gl_texture_bind(&texture_orginalBuffer, shader_projectPO_paramSrc, 0);
			gl_texture_bind(&texture_roadmap2, shader_projectPO_paramRoadmapT2, 1);
			gl_shader_setParam(shader_projectPO_paramMode, 4, gl_type_int, &mode);
			gl_frameBuffer_bind(&framebuffer_stageA, vSize, 1);
			gl_mesh_draw(NULL);
		}*/

		/* Blur the raw to remove noise */ {
			gl_shader_use(&shader_blur);
			gl_texture_bind(&texture_orginalBuffer, shader_blur_paramSrc, 0);
			gl_frameBuffer_bind(&framebuffer_raw[rri], vSize, 0);
			gl_mesh_draw(&mesh_persp);
		}

		/* Find changing pixel between two frame (pixel change means movement) */ {
			gl_shader_use(&shader_changingSensor);
			gl_texture_bind(&framebuffer_raw[rri].texture, shader_changingSensor_paramCurrent, 0);
			gl_texture_bind(&framebuffer_raw[rrj].texture, shader_changingSensor_paramPrevious, 1);
			gl_frameBuffer_bind(&framebuffer_stageA, vSize, 1);
			gl_mesh_draw(&mesh_persp);
		}

		/* Apply edge filter */ {
			gl_shader_use(&shader_edgeFilter);
			gl_texture_bind(&framebuffer_stageA.texture, shader_edgeFilter_paramSrc, 0);
			gl_frameBuffer_bind(&framebuffer_stageB, vSize, 1);
			gl_mesh_draw(&mesh_persp);
		}

		/* Refine edge, thinning the thick edge, only keep the lowest one */ {
			gl_shader_use(&shader_edgeRefine);
			gl_texture_bind(&framebuffer_stageB.texture, shader_edgeRefine_paramSrc, 0);
			gl_frameBuffer_bind(&framebuffer_stageA, vSize, 1);
			gl_mesh_draw(&mesh_persp);
		}

		/* Project from perspective to orthographic */ {
			ivec4 mode = {1, 0, 0, 0};
			gl_shader_use(&shader_projectPO);
			gl_texture_bind(&framebuffer_stageA.texture, shader_projectPO_paramSrc, 0);
			gl_texture_bind(&texture_roadmap2, shader_projectPO_paramRoadmapT2, 1);
			gl_shader_setParam(shader_projectPO_paramMode, 4, gl_type_int, &mode);
			gl_frameBuffer_bind(&framebuffer_edge[rri], vSize, 0);
			gl_mesh_draw(&mesh_ortho);
		}

		/* Determine edge */ {
			gl_shader_use(&shader_edgeDetermine);
			gl_texture_bind(&framebuffer_edge[rri].texture, shader_edgeDetermine_paramCurrent, 0);
			gl_texture_bind(&framebuffer_edge[rrj].texture, shader_edgeDetermine_paramPrevious, 1);
			gl_frameBuffer_bind(&framebuffer_stageA, vSize, 1);
			gl_mesh_draw(&mesh_ortho);
		}

		/* Measure the distance of edge moving between current frame and previous frame */ {
			gl_shader_use(&shader_distanceMeasure);
			gl_texture_bind(&framebuffer_stageA.texture, shader_distanceMeasure_paramEdgemap, 0);
			gl_texture_bind(&texture_roadmap1, shader_distanceMeasure_paramRoadmapT1, 1);
			gl_texture_bind(&texture_roadmap2, shader_distanceMeasure_paramRoadmapT2, 2);
			gl_frameBuffer_bind(&framebuffer_stageB, vSize, 1);
			gl_mesh_draw(&mesh_persp);
		}

		/* Project from orthographic back to perspective */ {
			ivec4 mode = {2, 0, 0, 0};
			gl_shader_use(&shader_projectPO);
			gl_texture_bind(&framebuffer_stageB.texture, shader_projectPO_paramSrc, 0);
			gl_texture_bind(&texture_roadmap2, shader_projectPO_paramRoadmapT2, 1);
			gl_shader_setParam(shader_projectPO_paramMode, 4, gl_type_int, &mode);
			gl_frameBuffer_bind(&framebuffer_speed, vSize, 0);
			gl_mesh_draw(&mesh_persp);
		}

		/* Sample the speed, denoise and avg */ {
			gl_shader_use(&shader_speedSample);
			gl_texture_bind(&framebuffer_speed.texture, shader_speedSample_paramSpeedmap, 0);
			gl_frameBuffer_bind(&framebuffer_stageA, vSize, 1);
			gl_mesh_draw(&mesh_speedsample);
		}

		/* Display the speed in human-readable text */ {
			gl_shader_use(&shader_speedometer);
			gl_texture_bind(&framebuffer_stageA.texture, shader_speedometer_paramSpeedmap, 0);
			gl_textureArray_bind(&texture_speedometer, shader_speedometer_paramGlyphmap, 1);
			gl_frameBuffer_bind(&framebuffer_display, vSize, 0);
			gl_mesh_draw(&mesh_speedometer);
		}

//		gl_fb* result = &framebuffer_stageA;
//		gl_fb* result = &framebuffer_stageB;
//		gl_fb* result = &framebuffer_raw[rri];
//		gl_fb* result = &framebuffer_edge[rri];
//		gl_fb* result = &framebuffer_speed;
		gl_fb* result = &framebuffer_display;
		gl_drawWindow(&texture_orginalBuffer, &result->texture);
	
		gl_synch barrier = gl_synchSet();
		if (gl_synchWait(barrier, GL_SYNCH_TIMEOUT) == gl_synch_timeout) { //timeout = 5e9 ns = 5s
			fputs("Render loop fatal stall\n", stderr);
			goto label_exit;
		}
		#ifdef DEBUG_THREADSPEED
			debug_threadSpeed = 'M'; //Not critical, no need to use mutex
		#endif
		gl_synchDelete(barrier);

		#ifdef VERBOSE
			char title[200];
			if (inBox(cursorPos, (size2d_t){0,0}, vSize, -1)) {
				vec4 color = gl_frameBuffer_getPixel(result, cursorPos);
				sprintf(title, "Viewer - frame %u, Cursor=(%zu,%zu), result=(%.3f,%.3f,%.3f,%.3f)", frameCnt, cursorPos.x, cursorPos.y, color.r, color.g, color.b, color.a);
			} else
				sprintf(title, "Viewer - frame %u, Cursor=(out) result=(none)", frameCnt);
			gl_drawEnd(title);
		#else
			char title[100];
			sprintf(title, "Viewer - frame %u", frameCnt);
			gl_drawEnd(title);
		#endif
		
		sem_wait(&sem_readerJobDone); //Wait reader thread finish uploading frame data
		#ifdef USE_PBO_UPLOAD
			gl_pixelBuffer_updateFinish();
		#endif

		#ifdef DEBUG_THREADSPEED
			fputc(debug_threadSpeed, stdout);
			fputs(" - ", stdout);
		#endif
		endTime = nanotime();
		fprintf(stdout, "Loop %u takes %lfms/frame.", frameCnt, (endTime - startTime) / 1e6);
		fflush(stdout);
	
		frameCnt++;

		#if (FRAME_DELAY != 0)
			if (frameCnt > FRAME_DEBUGSKIPSECOND * fps) usleep(FRAME_DELAY);
		#endif
	}



	/* Free all resources, house keeping */
	status = EXIT_SUCCESS;
label_exit:

	gl_shader_destroy(&shader_speedometer);
	gl_shader_destroy(&shader_distanceMeasure);
	gl_shader_destroy(&shader_edgeDetermine);
	gl_shader_destroy(&shader_projectPO);
	gl_shader_destroy(&shader_edgeRefine);
	gl_shader_destroy(&shader_edgeFilter);
	gl_shader_destroy(&shader_changingSensor);
	gl_shader_destroy(&shader_blur);
	gl_shader_destroy(&shader_roadmapCheck);

	gl_frameBuffer_delete(&framebuffer_stageB);
	gl_frameBuffer_delete(&framebuffer_stageA);
	gl_frameBuffer_delete(&framebuffer_display);
	gl_frameBuffer_delete(&framebuffer_speed);
	gl_frameBuffer_delete(&framebuffer_edge[1]);
	gl_frameBuffer_delete(&framebuffer_edge[0]);
	gl_frameBuffer_delete(&framebuffer_raw[1]);
	gl_frameBuffer_delete(&framebuffer_raw[0]);

	gl_texture_delete(&texture_speedometer);
	gl_mesh_delete(&mesh_speedometer);
	gl_mesh_delete(&mesh_speedsample);

	gl_texture_delete(&texture_roadmap2);
	gl_texture_delete(&texture_roadmap1);
	gl_mesh_delete(&mesh_ortho);
	gl_mesh_delete(&mesh_persp);

	if (th_reader_idValid) {
		pthread_cancel(th_reader_id);
		pthread_join(th_reader_id, NULL);
	}
	th_reader_idValid = 0;

	if (sem_validFlag & sem_validFlag_readerJobStart) {
		sem_destroy(&sem_readerJobStart);
		sem_validFlag &= ~sem_validFlag_readerJobStart;
	}
	if (sem_validFlag & sem_validFlag_readerJobDone) {
		sem_destroy(&sem_readerJobDone);
		sem_validFlag &= ~sem_validFlag_readerJobDone;
	}

	gl_texture_delete(&texture_orginalBuffer);
	#ifdef USE_PBO_UPLOAD
		gl_pixelBuffer_delete(&pbo[1]);
		gl_pixelBuffer_delete(&pbo[0]);
	#else
		free(rawData[0]);
		free(rawData[1]);
	#endif

	gl_destroy();

	fprintf(stdout, "\n%u frames displayed.\n\n", frameCnt);
	return status;
}