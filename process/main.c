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

/* Program config */
#define FIFONAME "tmpframefifo.data" //Video input stream
#define GL_SYNCH_TIMEOUT 5000000000LLU //For gl sync timeout

/* Debug timestamp */
#define FRAME_DELAY (50 * 1000)
#define FRAME_DEBUGSKIPSECOND 10

/* GPU buffer memory format */
#define INTERNAL_COLORFORMAT gl_texformat_RGBA32F //Must use RGBA16F or RGBA32F

/* Input video interleave */
#define INPUT_INTERLEAVE 1 //Read 1 frame for every X frame, used to skip some frames 

/* Edge post-process */
#define EDGE_DENOISE "0.1" //Noise threshold
#define EDGE_THRESHOLD_CURRENT 0.3 //Minimum width of object (m) on both side
#define EDGE_THRESHOLD_PREVIOUS 0.5 //Minimum width of object (m) on either side
#define EDGE_TOLERANCE_CURRENT 0.0 //Tolerance (tear width) (m)
#define EDGE_TOLERANCE_PREVIOUS 0.1

/* Speedometer */
#define SPEEDOMETER_FILE "./textmap.data"
#define SPEEDOMETER_COLORFORMAT gl_texformat_RGBA8
#define SPEEDOMETER_CNT (size2d_t){27, 30}

#define DEBUG_THREADSPEED
#ifdef DEBUG_THREADSPEED
volatile char debug_threadSpeed = ' '; //Which thread takes longer
#endif

//#define USE_PBO_UPLOAD

#define info(format, ...) {fprintf(stderr, "Log:\t"format"\n" __VA_OPT__(,) __VA_ARGS__);} //Write log
#define error(format, ...) {fprintf(stderr, "Err:\t"format"\n" __VA_OPT__(,) __VA_ARGS__);} //Write error log

/* -- Reader thread ------------------------------------------------------------------------- */

#define BLOCKSIZE 64 //Height and width are multiple of 8, so frame size is multiple of 64. Read a block (64px) at once can increase performance

struct {
	//Interface - Main thread side
	pthread_t tid; //Reader thread ID
	sem_t sem_readerStart; //Fired by main thread: when pointer to pbo is ready, reader can begin to upload
	sem_t sem_readerDone; //Fired by reader thread: when uploading is done, main thread can use
	volatile void volatile* rawDataPtr; //Video raw data goes here. Main thread write pointer here, reader thread put data into this address
	//Private - For reader thread function param passing
	struct { unsigned int r, g, b, a; } colorChannel; //Private, for reader read function
	unsigned int blockCnt; //Private, for reader read function
	FILE* fp; //Private, for reader read function
} th_reader_mangment;

struct th_reader_arg {
	unsigned int size; //Number of pixels in one frame
	const char* colorScheme; //Color scheme of the input raw data (numberOfChannel[1,3or4],orderOfChannelRGBA)
};

//Reader thread private function - reader thread
void* th_reader(void* arg);

/** Reader thread init.
 * Prepare semaphores, launch thread. 
 * @param size Size of video in px
 * @param colorScheme A string represents the color format
 * @param message Used to return error message in case this function fail
 * @param code Used to return error code in case this function fail
 * @return If success, return 1; if fail, release all resources and return 0
 */
int th_reader_init(const unsigned int size, const char* colorScheme, char** message, int* code) {
	if (sem_init(&th_reader_mangment.sem_readerStart, 0, 0)) {
		*code = errno;
		*message = "Fail to create main-reader master semaphore";
		return 0;
	}

	if (sem_init(&th_reader_mangment.sem_readerDone, 0, 0)) {
		*code = errno;
		*message = "Fail to create main-reader secondary semaphore";
		sem_destroy(&th_reader_mangment.sem_readerStart);
		return 0;
	}

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	struct th_reader_arg arg = {.size = size, .colorScheme = colorScheme};
	int err = pthread_create(&th_reader_mangment.tid, &attr, th_reader, &arg);
	if (err) {
		*code = err;
		*message = "Fail to create thread";
		sem_destroy(&th_reader_mangment.sem_readerDone);
		sem_destroy(&th_reader_mangment.sem_readerStart);
		return 0;
	}

	return 1;
}

/** Call this function when the main thread issue new address for video uploading. 
 * Reader will begin data uploading after this call. 
 * @param addr Address to upload video data
 */
void th_reader_newAddress(void* addr) {
	th_reader_mangment.rawDataPtr = addr;
	sem_post(&th_reader_mangment.sem_readerStart);
}

/** Call this function to block the main thread until reader thread finishing video reading. 
 */
void th_reader_waitReader() {
	sem_wait(&th_reader_mangment.sem_readerDone);
}

/** Terminate reader thread and release associate resources
 */
void th_reader_destroy() {
	pthread_cancel(th_reader_mangment.tid);
	pthread_join(th_reader_mangment.tid, NULL);

	sem_destroy(&th_reader_mangment.sem_readerDone);
	sem_destroy(&th_reader_mangment.sem_readerStart);
}

//Reader thread private function - read luma video
int th_reader_readLuma() {
	uint8_t* dest = (uint8_t*)th_reader_mangment.rawDataPtr;
	uint8_t luma[BLOCKSIZE];
	for (unsigned int i = th_reader_mangment.blockCnt; i; i--) { //Block count = frame size / block size
		if (!fread(luma, 1, BLOCKSIZE, th_reader_mangment.fp)) {
			return 0; //Fail to read, or end of file
		}
		for (uint8_t* p = luma; p < luma + BLOCKSIZE; p++) {
			*(dest++) = *p; //R
			*(dest++) = *p; //G
			*(dest++) = *p; //B
			*(dest++) = 0xFF; //A
		}
	}
	return 1;
}

//Reader thread private function - read RGB video
int th_reader_readRGB() {
	uint8_t* dest = (uint8_t*)th_reader_mangment.rawDataPtr;
	uint8_t rgb[BLOCKSIZE * 3];
	for (unsigned int i = th_reader_mangment.blockCnt; i; i--) {
		if (!fread(rgb, 3, BLOCKSIZE, th_reader_mangment.fp)) {
			return 0;
		}
		for (uint8_t* p = rgb; p < rgb + BLOCKSIZE * 3; p += 3) {
			*(dest++) = p[th_reader_mangment.colorChannel.r]; //R
			*(dest++) = p[th_reader_mangment.colorChannel.g]; //G
			*(dest++) = p[th_reader_mangment.colorChannel.b]; //B
			*(dest++) = 0xFF; //A
		}
	}
	return 1;
}

//Reader thread private function - read RGBA video
int th_reader_readRGBA() {
	uint8_t* dest = (uint8_t*)th_reader_mangment.rawDataPtr;
	uint8_t rgba[BLOCKSIZE * 4];
	for (unsigned int i = th_reader_mangment.blockCnt; i; i--) {
		if (!fread(rgba, 4, BLOCKSIZE, th_reader_mangment.fp)) {
			return 0;
		}
		for (uint8_t* p = rgba; p < rgba + BLOCKSIZE * 4; p += 4) {
			*(dest++) = p[th_reader_mangment.colorChannel.r]; //R
			*(dest++) = p[th_reader_mangment.colorChannel.g]; //G
			*(dest++) = p[th_reader_mangment.colorChannel.b]; //B
			*(dest++) = p[th_reader_mangment.colorChannel.a]; //A
		}
	}
	return 1;
}

//Reader thread private function - read RGBA video with channel = RGBA (in order)
int th_reader_readRGBADirect() {
	if (!fread((void*)th_reader_mangment.rawDataPtr, 4, th_reader_mangment.blockCnt, th_reader_mangment.fp)) //Block count = frame size / block size
		return 0;
	return 1;
}

//Reader thread private function - reader thread
void* th_reader(void* arg) {
	struct th_reader_arg* this = arg;
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
	int (*readFunction)();

	info("[Reader] Frame size: %u pixels. Number of color channel: %c", this->size, this->colorScheme[0]);

	if (this->colorScheme[1] != '\0') {
		th_reader_mangment.colorChannel.r = this->colorScheme[1] - '0';
		if (this->colorScheme[2] != '\0') {
			th_reader_mangment.colorChannel.g = this->colorScheme[2] - '0';
			if (this->colorScheme[3] != '\0') {
				th_reader_mangment.colorChannel.b = this->colorScheme[3] - '0';
				if (this->colorScheme[4] != '\0')
					th_reader_mangment.colorChannel.a = this->colorScheme[4] - '0';
			}
		}
	}
	if (this->colorScheme[0] == '1') {
		info("[Reader] Color scheme: RGB = idx0, A = 0xFF");
		th_reader_mangment.blockCnt = this->size / BLOCKSIZE;
		readFunction = th_reader_readLuma;
	} else if (this->colorScheme[0] == '3') {
		info("[Reader] Color scheme: R = idx%d, G = idx%d, B = idx%d, A = 0xFF", th_reader_mangment.colorChannel.r, th_reader_mangment.colorChannel.g, th_reader_mangment.colorChannel.b);
		th_reader_mangment.blockCnt = this->size / BLOCKSIZE;
		readFunction = th_reader_readRGB;
	} else {
		info("[Reader] Color scheme: R = idx%d, G = idx%d, B = idx%d, A = idx%d", th_reader_mangment.colorChannel.r, th_reader_mangment.colorChannel.g, th_reader_mangment.colorChannel.b, th_reader_mangment.colorChannel.a);
		if (th_reader_mangment.colorChannel.r != 0 || th_reader_mangment.colorChannel.g != 1 || th_reader_mangment.colorChannel.b != 2 || th_reader_mangment.colorChannel.a != 3) {
			th_reader_mangment.blockCnt = this->size / BLOCKSIZE;
			readFunction = th_reader_readRGBA;
		} else {
			readFunction = th_reader_readRGBADirect;
			th_reader_mangment.blockCnt = this->size;
		}
	}

	unlink(FIFONAME); //Delete if exist
	if (mkfifo(FIFONAME, 0777) == -1) {
		error("[Reader] Fail to create FIFO '"FIFONAME"' (errno = %d)", errno);
		return NULL;
	}
	info("[Reader] Ready. FIFO '"FIFONAME"' can accept frame data now");

	th_reader_mangment.fp = fopen(FIFONAME, "rb"); //Stall until some data writed into FIFO
	if (!th_reader_mangment.fp) {
		error("[Reader] Fail to open FIFO '"FIFONAME"' (errno = %d)", errno);
		unlink(FIFONAME);
		return NULL;
	}
	info("[Reader] FIFO '"FIFONAME"' data received");

	int cont = 1;
	while (cont) {
		sem_wait(&th_reader_mangment.sem_readerStart); //Wait until main thread issue new memory address for next frame

		if (!readFunction())
			cont = 0;

		#ifdef DEBUG_THREADSPEED
			debug_threadSpeed = 'R';
		#endif
		
		sem_post(&th_reader_mangment.sem_readerDone); //Uploading done, allow main thread to use it
	}

	fclose(th_reader_mangment.fp);
	unlink(FIFONAME);
	info("[Reader] End of file or broken pipe! Please close the viewer window to terminate the program");

	while (1) { //Send dummy data to keep the main thread running
		sem_wait(&th_reader_mangment.sem_readerStart); //Wait until main thread issue new memory address for next frame
		
		error("SSS");
		memset((void*)th_reader_mangment.rawDataPtr, 0, this->size * 4);
		error("DDD");

		#ifdef DEBUG_THREADSPEED
			debug_threadSpeed = 'R';
		#endif

		sem_post(&th_reader_mangment.sem_readerDone); //Uploading done, allow main thread to use it
	}

	return NULL;
}

#undef BLOCKSIZE

/* -- Main thread --------------------------------------------------------------------------- */

/** Check the result of shader program creation. 
 * @param name For display purpose
 * @param program The program returned by shader program creation
 * @param srcs The srcs list used for shader program creation
 * @param args The args list used for shader program creation
 * @param stream Where to write the log
 * @return 0 if shader creation is fail or some arguments are missing in the shader; 1 if success
 */
int checkShaderProgram(const char* name, const gl_program program, const gl_programSrc* srcs, const gl_programArg* args, const FILE* stream);

int main(int argc, char* argv[]) {
	const unsigned int zeros[4] = {0, 0, 0, 0}; //Zero array with 4 elements (can be used as zeros[1], zeros[2] or zeros[3] as well, it is just a pointer in C)

	int status = EXIT_FAILURE;
	unsigned int frameCnt = 0;
	
	unsigned int sizeData[3]; //Number of pixels in width and height in the input video, depth/leave is 0 (convenient for texture creation which requires size[3])
	const char* color; //Input video color scheme
	unsigned int fps; //Input video FPS
	const char* roadmapFile; //File dir - roadmap file (binary)

	/* Program argument check */ {
		if (argc != 6) {
			error("Bad arg: Use 'this width height fps color roadmapFile'");
			error("\twhere color = ncccc (n is number of channel input, cccc is the order of RGB[A])");
			error("\troadmappFile = Directory to a binary coded file contains road-domain data");
			return status;
		}
		sizeData[0] = atoi(argv[1]);
		sizeData[1] = atoi(argv[2]);
		fps = atoi(argv[3]);
		color = argv[4];
		roadmapFile = argv[5];
		info("Start...\n");
		info("\tWidth: %upx, Height: %upx, Total: %usqpx", sizeData[0], sizeData[1], sizeData[0] * sizeData[1]);
		info("\tFPS: %u, Color: %s", fps, color);
		info("\tRoadmap: %s", roadmapFile);

		if (sizeData[0] & (unsigned int)0b111 || sizeData[0] < 320 || sizeData[0] > 4056) {
			error("Bad width: Width must be multiple of 8, 320 <= width <= 4056");
			return status;
		}
		if (sizeData[1] & (unsigned int)0b111 || sizeData[1] < 240 || sizeData[1] > 3040) {
			error("Bad height: Height must be multiple of 8, 240 <= height <= 3040");
			return status;
		}
		if (color[0] != '1' && color[0] != '3' && color[0] != '4') {
			error("Bad color: Color channel must be 1, 3 or 4");
			return status;
		}
		if (strnlen(color, 10) != color[0] - '0' + 1) {
			error("Bad color: Color scheme and channel count mismatched");
			return status;
		}
		for (int i = 1; i < strlen(color); i++) {
			if (color[i] < '0' || color[i] >= color[0]) {
				error("Bad color: Each color scheme must be in the range of [0, channel-1 (%d)]", color[0] - '0' - 1);
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

	//Final display on screen
	gl_mesh mesh_final = GL_INIT_DEFAULT_MESH;

	//Work stages: To save intermediate results during the process
	typedef struct Framebuffer {
		gl_fbo fbo;
		gl_tex tex;
	} fb;
	#define DEFAULT_FB (fb){GL_INIT_DEFAULT_FBO, GL_INIT_DEFAULT_TEX}
	fb fb_raw[2] = {DEFAULT_FB, DEFAULT_FB}; //Raw video data with minor pre-process
//	fb fb_moveEdge = DEFAULT_FB; //Moving part and edge detection of current frame
	fb fb_object[4] = {DEFAULT_FB, DEFAULT_FB, DEFAULT_FB, DEFAULT_FB}; //Object detection of current previous frames
	fb fb_speed = DEFAULT_FB; //Displacement of two moving edge (speed of moving edge)
	fb fb_display = DEFAULT_FB; //Display human-readable text
	fb fb_stageA = DEFAULT_FB;
	fb fb_stageB = DEFAULT_FB;

	//Program - Roadmap check
	struct {
		gl_program pid;
		gl_param roadmapT1;
		gl_param roadmapT2;
		gl_param cfgI1;
		gl_param cfgF1;
	} program_roadmapCheck = {.pid = GL_INIT_DEFAULT_PROGRAM};
	enum program_roadmapCheck_mode {
		program_roadmapCheck_modeShowT1 = 1,
		program_roadmapCheck_modeShowT2 = 2,
		program_roadmapCheck_modeShowPerspGrid = 3,
		program_roadmapCheck_modeShowOrthoGrid = 4
	};

	//Program - Project perspective to orthographic
	struct {
		gl_program pid;
		gl_param src;
		gl_param roadmapT2;
	} program_projectP2O = {.pid = GL_INIT_DEFAULT_PROGRAM};
	//Program - Project orthographic to perspective
	struct {
		gl_program pid;
		gl_param src;
		gl_param roadmapT2;
	} program_projectO2P = {.pid = GL_INIT_DEFAULT_PROGRAM};

	//Program - Blur filter
	struct {
		gl_program pid;
		gl_param src;
	} program_blurFilter = {.pid = GL_INIT_DEFAULT_PROGRAM};
	//Program - Edge filter
	struct {
		gl_program pid;
		gl_param src;
	} program_edgeFilter = {.pid = GL_INIT_DEFAULT_PROGRAM};

	//Program - Changing sensor
	struct {
		gl_program pid;
		gl_param current;
		gl_param previous;
	} program_changingSensor = {.pid = GL_INIT_DEFAULT_PROGRAM};

	//Program - Move and edge detection
	/*struct {
		gl_program pid;
		gl_param current;
		gl_param previous;
	} program_moveEdge = {.pid = GL_INIT_DEFAULT_PROGRAM};*/

	//Program - Object fix
	struct {
		gl_program pid;
		gl_param src;
		gl_param roadmapT1;
	} program_objectFix[2] = {{.pid = GL_INIT_DEFAULT_PROGRAM}, {.pid = GL_INIT_DEFAULT_PROGRAM}};

	//Program - Edge refine
	struct {
		gl_program pid;
		gl_param src;
	} program_edgeRefine = {.pid = GL_INIT_DEFAULT_PROGRAM};

	//Program - Measure
	struct {
		gl_program pid;
		gl_param current;
		gl_param previous;
		gl_param roadmapT1;
		gl_param roadmapT2;
	} program_measure = {.pid = GL_INIT_DEFAULT_PROGRAM};
	gl_program shader_measure = GL_INIT_DEFAULT_PROGRAM;

	//Program - Edge determine
	gl_program shader_edgeDetermine = GL_INIT_DEFAULT_PROGRAM;
	gl_param shader_edgeDetermine_paramCurrent;
	gl_param shader_edgeDetermine_paramPrevious;

	//Program - Edge fix
	gl_program shader_edgeFix = GL_INIT_DEFAULT_PROGRAM;
	gl_param shader_edgeFix_paramEdgemap;

	//Program - Distance measure
	gl_program shader_distanceMeasure = GL_INIT_DEFAULT_PROGRAM;
	gl_param shader_distanceMeasure_paramEdgemap;
	gl_param shader_distanceMeasure_paramRoadmapT1;
	gl_param shader_distanceMeasure_paramRoadmapT2;

	//Program - Speed sample
	gl_program shader_speedSample = GL_INIT_DEFAULT_PROGRAM;
	gl_param shader_speedSample_paramSpeedmap;

	//Program - Speedometer
	gl_program shader_speedometer = GL_INIT_DEFAULT_PROGRAM;
	gl_param shader_speedometer_paramSpeedmap;
	gl_param shader_speedometer_paramGlyphmap;

	//Program - Final
	struct {
		gl_program pid;
		gl_param orginal;
		gl_param result;
	} program_final = {.pid = GL_INIT_DEFAULT_PROGRAM};

	/* Init OpenGL and viewer window */ {
		info("Init openGL...");
		gl_logStream(stderr);
		if (!gl_init()) {
			error("Cannot init OpenGL");
			goto label_exit;
		}
	}

	/* Use a texture to store raw frame data & Start reader thread */ {
		info("Prepare video upload buffer...");
		#ifdef USE_PBO_UPLOAD
			pbo[0] = gl_pixelBuffer_create(sizeData[0] * sizeData[1] * 4); //Always use RGBA8 (good performance)
			pbo[1] = gl_pixelBuffer_create(sizeData[0] * sizeData[1] * 4);
			if (!gl_pixelBuffer_check(&(pbo[0])) || !gl_pixelBuffer_check(&(pbo[1]))) {
				error("Fail to create pixel buffers for orginal frame data uploading");
				goto label_exit;
			}
		#else
			rawData[0] = malloc(sizeData[0] * sizeData[1] * 4); //Always use RGBA8 (aligned, good performance)
			rawData[1] = malloc(sizeData[0] * sizeData[1] * 4);
			if (!rawData[0] || !rawData[1]) {
				error("Fail to create memory buffers for orginal frame data loading");
				goto label_exit;
			}
		#endif

		texture_orginalBuffer = gl_texture_create(gl_texformat_RGBA8, gl_textype_2d, sizeData);
		if (!gl_texture_check(&texture_orginalBuffer)) {
			error("Fail to create texture buffer for orginal frame data storage");
			goto label_exit;
		}

		info("Init reader thread...");
		char* message;
		int code;
		if (!th_reader_init(sizeData[0] * sizeData[1], color, &message, &code)) {
			error("Fail to create chile thread: %s. Error code %d", message, code);
			goto label_exit;
		}
	}

	/* Roadmap: mesh for focus region, road info, textures for road data */ {
		info("Load resource - roadmap \"%s\"...", roadmapFile);
		char* statue;
		Roadmap roadmap = roadmap_init(roadmapFile, &statue);
		if (!roadmap) {
			error("Cannot load roadmap: %s", statue);
			goto label_exit;
		}
		
		roadinfo = roadmap_getHeader(roadmap);
		unsigned int sizeRoadmap[3] = {roadinfo.width, roadinfo.height, 0};

		/* Focus region */ {
			gl_index_t attributes[] = {2};
			unsigned int vCnt;
			float (* vertices)[2] = (float(*)[2])roadmap_getRoadPoints(roadmap, &vCnt);

			float outLeft = 1.0f, outRight = 0.0f, outTop = 1.0f, outBottom = 0.0f;
			for (unsigned int i = 0; i < vCnt; i++) {
				outLeft = fminf(outLeft, vertices[i][0]);
				outRight = fmaxf(outRight, vertices[i][0]);
				outTop = fminf(outTop, vertices[i][1]);
				outBottom = fmaxf(outBottom, vertices[i][1]);
			}
			float vOthor[4][2] = {
				{outLeft, outTop},
				{outRight, outTop},
				{outRight, outBottom},
				{outLeft, outBottom}
			};

			mesh_persp = gl_mesh_create((const unsigned int[3]){2, vCnt, 0}, attributes, (gl_vertex_t*)vertices, NULL, gl_meshmode_triangleStrip, gl_usage_static);
			mesh_ortho = gl_mesh_create((const unsigned int[3]){2, 4, 0}, attributes, (gl_vertex_t*)vOthor, NULL, gl_meshmode_triangleFan, gl_usage_static);
			if ( !gl_mesh_check(&mesh_persp) || !gl_mesh_check(&mesh_ortho) ) {
				roadmap_destroy(roadmap);
				error("Fail to create mesh for roadmap - focus region");
				goto label_exit;
			}
		}
		
		/* Road data */ {
			texture_roadmap1 = gl_texture_create(gl_texformat_RGBA32F, gl_textype_2d, sizeRoadmap);
			texture_roadmap2 = gl_texture_create(gl_texformat_RGBA32I, gl_textype_2d, sizeRoadmap);
			if (!gl_texture_check(&texture_roadmap1) || !gl_texture_check(&texture_roadmap2)) {
				roadmap_destroy(roadmap);
				error("Fail to create texture buffer for roadmap - road-domain data storage");
				goto label_exit;
			}

			gl_texture_update(&texture_roadmap1, roadmap_getT1(roadmap), zeros, sizeRoadmap);
			gl_texture_update(&texture_roadmap2, roadmap_getT2(roadmap), zeros, sizeRoadmap);
		}

		gl_synch barrier = gl_synchSet();
		if (gl_synchWait(barrier, GL_SYNCH_TIMEOUT) == gl_synch_timeout) {
			error("Roadmap GPU uploading fatal stall");
			gl_synchDelete(barrier);
			roadmap_destroy(roadmap);
			goto label_exit;
		}
		gl_synchDelete(barrier);

		roadmap_destroy(roadmap); //Free roadmap memory after data uploading finished
	}

	/* [TODO] Speedometer: speedometer sample region, display location and glphy texture */ {
		fputs("Load resource - speedometer...\n", stdout);
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
		texture_speedometer = gl_texture_create(SPEEDOMETER_COLORFORMAT, gl_textype_2dArray, (const unsigned int[3]){glyphSize.width, glyphSize.height, 256});
		if (!gl_texture_check(&texture_speedometer)) {
			fputs("Fail to create texture buffer for speedometer\n", stderr);
			goto label_exit;
		}
		gl_texture_update(&texture_speedometer, glyph, zeros, (const unsigned int[3]){glyphSize.width, glyphSize.height, 256});

		vec2 speedometer_size = {1.0f / speedometer_cnt.width, 1.0f / speedometer_cnt.height};
		gl_index_t attribute[] = {4};
		vec4 vSample[speedometer_cnt.height][speedometer_cnt.width];
		for (int y = 0; y < speedometer_cnt.height; y++) {
			for (int x = 0; x < speedometer_cnt.width; x++) {
				vSample[y][x] = (vec4){
					.x = speedometer_size.x * x,
					.y = speedometer_size.y * y,
					.z = speedometer_size.x * x + speedometer_size.x,
					.w = speedometer_size.y * y + speedometer_size.y
				};
			}
		}
		mesh_speedsample = gl_mesh_create((const unsigned int[3]){4, speedometer_cnt.height * speedometer_cnt.width, 0}, attribute, (gl_vertex_t*)vSample, NULL, gl_meshmode_points, gl_usage_static);
		if (!gl_mesh_check(&mesh_speedsample)) {
			speedometer_destroy(speedometer);
			fputs("Fail to create mesh for speedometer\n", stderr);
			goto label_exit;
		}

		vec4 vMeter[speedometer_cnt.height][speedometer_cnt.width][6];
		for (int y = 0; y <= speedometer_cnt.height; y++) {
			for (int x = 0; x <= speedometer_cnt.width; x++) {
				float left = speedometer_size.x * x, right = speedometer_size.x * x + speedometer_size.x;
				float top = speedometer_size.y * y, bottom = speedometer_size.y * y + speedometer_size.y;
				float center = (left + right) / 2, middle = (top + bottom) / 2;
				vMeter[y][x][0] = (vec4){	.x=left,	.y=top,		.z=center,	.w=middle	};
				vMeter[y][x][1] = (vec4){	.x=right,	.y=top,		.z=center,	.w=middle	};
				vMeter[y][x][2] = (vec4){	.x=left,	.y=bottom,	.z=center,	.w=middle	};
				vMeter[y][x][3] = (vec4){	.x=right,	.y=top,		.z=center,	.w=middle	};
				vMeter[y][x][4] = (vec4){	.x=left,	.y=bottom,	.z=center,	.w=middle	};
				vMeter[y][x][5] = (vec4){	.x=right,	.y=bottom,	.z=center,	.w=middle	};
			}
		}
		mesh_speedometer = gl_mesh_create((const unsigned int[3]){4, speedometer_cnt.height * speedometer_cnt.width * 6, 0}, attribute, (gl_vertex_t*)vMeter, NULL, gl_meshmode_triangles, gl_usage_static);
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
		info("Create GPU buffers...");

		for (unsigned int i = 0; i < sizeof(fb_raw) / sizeof(fb_raw[0]); i++) {
			fb_raw[i].tex = gl_texture_create(gl_texformat_RGBA8, gl_textype_2d, sizeData);
			if (!gl_texture_check(&fb_raw[i].tex)) {
				error("Fail to create texture to store raw video data (%u)", i);
				goto label_exit;
			}
			fb_raw[i].fbo = gl_frameBuffer_create(&fb_raw[i].tex, 1);
			if (!gl_frameBuffer_check(&fb_raw[i].fbo) ) {
				error("Fail to create FBO to store raw video data (%u)", i);
				goto label_exit;
			}
		}

/*		fb_moveEdge.tex = gl_texture_create(gl_texformat_RGBA8, gl_textype_2d, sizeData);
		if (!gl_texture_check(&fb_moveEdge.tex)) {
			fputs("Fail to create texture to store move and edge\n", stderr);
			goto label_exit;
		}
		fb_moveEdge.fbo = gl_frameBuffer_create(&fb_moveEdge.tex, 1);
		if (!gl_frameBuffer_check(&fb_moveEdge.fbo)) {
			fputs("Fail to create FBO to store move and edge\n", stderr);
			goto label_exit;
		}*/

		for (unsigned int i = 0; i < sizeof(fb_object) / sizeof(fb_object[0]); i++) {
			fb_object[i].tex = gl_texture_create(gl_texformat_RGBA8, gl_textype_2d, sizeData);
			if (!gl_texture_check(&fb_object[i].tex)) {
				fprintf(stderr, "Fail to create texture to store object (%u)\n", i);
				goto label_exit;
			}
			fb_object[i].fbo = gl_frameBuffer_create(&fb_object[i].tex, 1);
			if (!gl_frameBuffer_check(&fb_object[i].fbo) ) {
				fprintf(stderr, "Fail to create FBO to store object (%u)\n", i);
				goto label_exit;
			}
		}

		fb_speed.tex = gl_texture_create(INTERNAL_COLORFORMAT, gl_textype_2d, sizeData);
		if (!gl_texture_check(&fb_speed.tex)) {
			fputs("Fail to create texture to store speed\n", stderr);
			goto label_exit;
		}
		fb_speed.fbo = gl_frameBuffer_create(&fb_speed.tex, 1);
		if (!gl_frameBuffer_check(&fb_speed.fbo) ) {
			fputs("Fail to create FBO to store speed\n", stderr);
			goto label_exit;
		}

		fb_display.tex = gl_texture_create(gl_texformat_RGBA8, gl_textype_2d, sizeData);
		if (!gl_texture_check(&fb_display.tex)) {
			fputs("Fail to create texture to store speed\n", stderr);
			goto label_exit;
		}
		fb_display.fbo = gl_frameBuffer_create(&fb_display.tex, 1);
		if (!gl_frameBuffer_check(&fb_display.fbo) ) {
			fputs("Fail to create FBO to store speed\n", stderr);
			goto label_exit;
		}

		fb_stageA.tex = gl_texture_create(INTERNAL_COLORFORMAT, gl_textype_2d, sizeData);
		fb_stageB.tex = gl_texture_create(INTERNAL_COLORFORMAT, gl_textype_2d, sizeData);
		if ( !gl_texture_check(&fb_stageA.tex) || !gl_texture_check(&fb_stageB.tex) ) {
			fputs("Fail to create texture to store moving edge\n", stderr);
			goto label_exit;
		}
		fb_stageA.fbo = gl_frameBuffer_create(&fb_stageA.tex, 1);
		fb_stageB.fbo = gl_frameBuffer_create(&fb_stageB.tex, 1);
		if ( !gl_frameBuffer_check(&fb_stageA.fbo) || !gl_frameBuffer_check(&fb_stageB.fbo) ) {
			fputs("Fail to create FBO to store moving edge\n", stderr);
			goto label_exit;
		}
	}

	/* Create final display mesh */ {
		gl_vertex_t vertices[] = {
			1.0f, 0.0f,
			1.0f, 1.0f,
			0.0f, 1.0f,
			0.0f, 0.0f
		};
		gl_index_t indices[] = {0, 3, 2, 0, 2, 1};
		gl_index_t attributes[] = {2};
		mesh_final = gl_mesh_create((const unsigned int[3]){2, 4, 6}, attributes, vertices, indices, gl_meshmode_triangles, gl_usage_static);
		if (!gl_mesh_check(&mesh_final)) {
			fputs("Fail to create mesh for final display\n", stderr);
			goto label_exit;
		}
	}

	/* Load shader programs */ {
		fputs("Load shaders...\n", stdout);
		#define NL "\n"
		const char* glShaderHeader = "#version 310 es"NL"precision mediump float;"NL;

		/* Create program: Roadmap check */ {
			gl_programSrc src[] = {
				{gl_programSrcType_vertex,	gl_programSrcLoc_mem,	glShaderHeader},
				{gl_programSrcType_vertex,	gl_programSrcLoc_file,	"shader/focusRegion.vs.glsl"},
				{gl_programSrcType_fragment,	gl_programSrcLoc_mem,	glShaderHeader},
				{gl_programSrcType_fragment,	gl_programSrcLoc_file,	"shader/roadmapCheck.fs.glsl"},
				{.str = NULL}
			};
			gl_programArg arg[] = {
				{gl_programArgType_normal,	"roadmapT1"},
				{gl_programArgType_normal,	"roadmapT2"},
				{gl_programArgType_normal,	"cfgI1"},
				{gl_programArgType_normal,	"cfgF1"},
				{.name = NULL}
			};

			program_roadmapCheck.pid = gl_program_create(src, arg);
			program_roadmapCheck.roadmapT1 = arg[0].id;
			program_roadmapCheck.roadmapT2 = arg[1].id;
			program_roadmapCheck.cfgI1 = arg[2].id;
			program_roadmapCheck.cfgF1 = arg[3].id;
			if (!checkShaderProgram("Roadmap check", program_roadmapCheck.pid, src, arg, stderr))
				goto label_exit;
		}

		/* Create program: Project P2O and P2O */ {
			const char* modeP2O = "#define P2O"NL;
			const char* modeO2P = "#define O2P"NL;

			gl_programSrc src[] = {
				{gl_programSrcType_vertex,	gl_programSrcLoc_mem,	glShaderHeader},
				{gl_programSrcType_vertex,	gl_programSrcLoc_file,	"shader/focusRegion.vs.glsl"},
				{gl_programSrcType_fragment,	gl_programSrcLoc_mem,	glShaderHeader},
				{gl_programSrcType_fragment,	gl_programSrcLoc_mem,	NULL},
				{gl_programSrcType_fragment,	gl_programSrcLoc_file,	"shader/projectPO.fs.glsl"},
				{.str = NULL}
			};
			gl_programArg arg[] = {
				{gl_programArgType_normal,	"src"},
				{gl_programArgType_normal,	"roadmapT2"},
				{.name = NULL}
			};

			src[3].str = modeP2O;
			program_projectP2O.pid = gl_program_create(src, arg);
			program_projectP2O.src = arg[0].id;
			program_projectP2O.roadmapT2 = arg[1].id;
			if (!checkShaderProgram("Project perspective to orthographic", program_projectP2O.pid, src, arg, stderr))
				goto label_exit;

			src[3].str = modeO2P;
			program_projectO2P.pid = gl_program_create(src, arg);
			program_projectO2P.src = arg[0].id;
			program_projectO2P.roadmapT2 = arg[1].id;
			if (!checkShaderProgram("Project orthographic to perspective", program_projectO2P.pid, src, arg, stderr))
				goto label_exit;
		}

		/* Create program: Blur and edge filter*/ {
			const char* blur =
				"const struct Mask {float v; ivec2 idx;} mask[] = Mask[]("NL
				"	Mask(1.0 / 16.0, ivec2(-1, -1)),	Mask(2.0 / 16.0, ivec2( 0, -1)),	Mask(1.0 / 16.0, ivec2(+1, -1)),"NL
				"	Mask(2.0 / 16.0, ivec2(-1,  0)),	Mask(4.0 / 16.0, ivec2( 0,  0)),	Mask(2.0 / 16.0, ivec2(+1,  0)),"NL
				"	Mask(1.0 / 16.0, ivec2(-1, +1)),	Mask(2.0 / 16.0, ivec2( 0, +1)),	Mask(1.0 / 16.0, ivec2(+1, +1))"NL
				");"NL
			;
			const char* edge =
				"#define CLAMP_L vec4(vec3(0.0), 1.0)"NL
				"#define CLAMP_H vec4(vec3(1.0), 1.0)"NL
				"const struct Mask {float v; ivec2 idx;} mask[] = Mask[]("NL
				"	Mask(-1.0, ivec2(0, -2)),"NL
				"	Mask(-2.0, ivec2(0, -1)),"NL
				"	Mask(+6.0, ivec2(0,  0)),"NL
				"	Mask(-2.0, ivec2(0, +1)),"NL
				"	Mask(-1.0, ivec2(0, +2))"NL
				");"NL
			;

			gl_programSrc src[] = {
				{gl_programSrcType_vertex,	gl_programSrcLoc_mem,	glShaderHeader},
				{gl_programSrcType_vertex,	gl_programSrcLoc_file,	"shader/focusRegion.vs.glsl"},
				{gl_programSrcType_fragment,	gl_programSrcLoc_mem,	glShaderHeader},
				{gl_programSrcType_fragment,	gl_programSrcLoc_mem,	NULL},
				{gl_programSrcType_fragment,	gl_programSrcLoc_file,	"shader/kernel.fs.glsl"},
				{.str = NULL}
			};
			gl_programArg arg[] = {
				{gl_programArgType_normal,	"src"},
				{.name = NULL}
			};

			src[3].str = blur;
			program_blurFilter.pid = gl_program_create(src, arg);
			program_blurFilter.src = arg[0].id;
			if (!checkShaderProgram("Blur filter", program_blurFilter.pid, src, arg, stderr))
				goto label_exit;

			src[3].str = edge;
			program_edgeFilter.pid = gl_program_create(src, arg);
			program_edgeFilter.src = arg[0].id;
			if (!checkShaderProgram("Edge filter", program_edgeFilter.pid, src, arg, stderr))
				goto label_exit;
		}
		
		/* Create program: Changing sensor */ {
			const char* cfg =
				"#define MONO"NL
//				"#define HUE"NL
				"#define BINARY vec4(vec3(0.05), -1.0)"NL
//				"#define CLAMP vec4[2](vec4(vec3(0.0), 1.0), vec4(vec3(1.0), 1.0))"NL
			;

			gl_programSrc src[] = {
				{gl_programSrcType_vertex,	gl_programSrcLoc_mem,	glShaderHeader},
				{gl_programSrcType_vertex,	gl_programSrcLoc_file,	"shader/focusRegion.vs.glsl"},
				{gl_programSrcType_fragment,	gl_programSrcLoc_mem,	glShaderHeader},
				{gl_programSrcType_fragment,	gl_programSrcLoc_mem,	cfg},
				{gl_programSrcType_fragment,	gl_programSrcLoc_file,	"shader/changingSensor.fs.glsl"},
				{.str = NULL}
			};
			gl_programArg arg[] = {
				{gl_programArgType_normal,	"current"},
				{gl_programArgType_normal,	"previous"},
				{.name = NULL}
			};

			program_changingSensor.pid = gl_program_create(src, arg);
			program_changingSensor.current = arg[0].id;
			program_changingSensor.previous = arg[1].id;
			if (!checkShaderProgram("Changing sensor", program_changingSensor.pid, src, arg, stderr))
				goto label_exit;
		}

		/* Create program: Move and edge detection */ /*{
			gl_programSrc src[] = {
				{gl_programSrcType_vertex,	gl_programSrcLoc_mem,	glShaderHeader},
				{gl_programSrcType_vertex,	gl_programSrcLoc_file,	"shader/focusRegion.vs.glsl"},
				{gl_programSrcType_fragment,	gl_programSrcLoc_mem,	glShaderHeader},
				{gl_programSrcType_fragment,	gl_programSrcLoc_file,	"shader/moveEdge.fs.glsl"},
				{.str = NULL}
			};
			gl_programArg arg[] = {
				{gl_programArgType_normal,	"current"},
				{gl_programArgType_normal,	"previous"},
				{.name = NULL}
			};

			program_moveEdge.pid = gl_program_create(src, arg);
			program_moveEdge.current = arg[0].id;
			program_moveEdge.previous = arg[1].id;
			if (!checkShaderProgram("Move and edge detection", program_moveEdge.pid, src, arg, stderr))
				goto label_exit;
		}*/

		/* Create program: Object fix (2 stages) */ {
			char* horizontal =
				"#define HORIZONTAL"NL
				"#define SEARCH_DISTANCE 0.7"NL
			;
			char* vertical =
				"#define VERTICAL"NL
				"#define SEARCH_DISTANCE 1.0"NL
			;

			gl_programSrc src[] = {
				{gl_programSrcType_vertex,	gl_programSrcLoc_mem,	glShaderHeader},
				{gl_programSrcType_vertex,	gl_programSrcLoc_file,	"shader/focusRegion.vs.glsl"},
				{gl_programSrcType_fragment,	gl_programSrcLoc_mem,	glShaderHeader},
				{gl_programSrcType_fragment,	gl_programSrcLoc_mem,	NULL},
				{gl_programSrcType_fragment,	gl_programSrcLoc_file,	"shader/objectFix.fs.glsl"},
				{.str = NULL}
			};
			gl_programArg arg[] = {
				{gl_programArgType_normal,	"src"},
				{gl_programArgType_normal,	"roadmapT1"},
				{.name = NULL}
			};

			src[3].str = horizontal;
			program_objectFix[0].pid = gl_program_create(src, arg);
			program_objectFix[0].src = arg[0].id;
			program_objectFix[0].roadmapT1 = arg[1].id;
			if (!checkShaderProgram("Object fix - Stage 1 horizontal", program_objectFix[0].pid, src, arg, stderr))
				goto label_exit;

			src[3].str = vertical;
			program_objectFix[1].pid = gl_program_create(src, arg);
			program_objectFix[1].src = arg[0].id;
			program_objectFix[1].roadmapT1 = arg[1].id;
			if (!checkShaderProgram("Object fix - Stage 2 vertical", program_objectFix[1].pid, src, arg, stderr))
				goto label_exit;
		}

		/* Create program: Edge refine */ {
			const char* cfg =
				"#define DENOISE_BOTTOM 5"NL
				"#define DENOISE_SIDE 20"NL
			;

			gl_programSrc src[] = {
				{gl_programSrcType_vertex,	gl_programSrcLoc_mem,	glShaderHeader},
				{gl_programSrcType_vertex,	gl_programSrcLoc_file,	"shader/focusRegion.vs.glsl"},
				{gl_programSrcType_fragment,	gl_programSrcLoc_mem,	glShaderHeader},
				{gl_programSrcType_fragment,	gl_programSrcLoc_mem,	cfg},
				{gl_programSrcType_fragment,	gl_programSrcLoc_file,	"shader/edgeRefine.fs.glsl"},
				{.str = NULL}
			};
			gl_programArg arg[] = {
				{gl_programArgType_normal,	"src"},
				{.name = NULL}
			};

			program_edgeRefine.pid = gl_program_create(src, arg);
			program_edgeRefine.src = arg[0].id;
			if (!checkShaderProgram("Edge refine", program_edgeRefine.pid, src, arg, stderr))
				goto label_exit;
		}

		/* Create program: Measure */ {
			char cfg[100];
			float cfgBiasOffset = 0.0f;
			float cfgBiasFirstOrder = fps * INPUT_INTERLEAVE * 3.6f;
			sprintf(cfg, "#define BIAS vec4(%.3f, %.3f, %.3f, %.3f)"NL, cfgBiasOffset, cfgBiasFirstOrder, 0.0f, 0.0f);

			gl_programSrc src[] = {
				{gl_programSrcType_vertex,	gl_programSrcLoc_mem,	glShaderHeader},
				{gl_programSrcType_vertex,	gl_programSrcLoc_file,	"shader/focusRegion.vs.glsl"},
				{gl_programSrcType_fragment,	gl_programSrcLoc_mem,	glShaderHeader},
				{gl_programSrcType_fragment,	gl_programSrcLoc_mem,	cfg},
				{gl_programSrcType_fragment,	gl_programSrcLoc_file,	"shader/measure.fs.glsl"},
				{.str = NULL}
			};
			gl_programArg arg[] = {
				{gl_programArgType_normal,	"current"},
				{gl_programArgType_normal,	"previous"},
				{gl_programArgType_normal,	"roadmapT1"},
				{gl_programArgType_normal,	"roadmapT2"},
				{.name = NULL}
			};

			program_measure.pid = gl_program_create(src, arg);
			program_measure.current = arg[0].id;
			program_measure.previous = arg[1].id;
			program_measure.roadmapT1 = arg[2].id;
			program_measure.roadmapT2 = arg[3].id;
			if (!checkShaderProgram("Measure", program_measure.pid, src, arg, stderr))
				goto label_exit;
		}

		

#if 1==2

		

		/* Create program: Edge determine */ {
			char cfgThreshold[100];
			int cfgThresholdCurrent = EDGE_THRESHOLD_CURRENT / roadinfo.orthoPixelWidth;
			int cfgThresholdPrevious = EDGE_THRESHOLD_PREVIOUS / roadinfo.orthoPixelWidth;
			sprintf(cfgThreshold, "const ivec4 threshold = ivec4(%d, %d, %d, %d);"NL, cfgThresholdCurrent, cfgThresholdPrevious, 0, 0);

			gl_shaderSrc vs[] = {
				{.isFile = 0, .src = glShaderHeader},
				{.isFile = 0, .src = glShaderLine0}, {.isFile = 1, .src = "shader/focusRegion.vs.glsl"}
			};
			gl_shaderSrc fs[] = {
				{.isFile = 0, .src = glShaderHeader},
				{.isFile = 0, .src = cfgThreshold},
				{.isFile = 0, .src = glShaderLine0}, {.isFile = 1, .src = "shader/edgeDetermine.fs.glsl"}
			};
			gl_shaderArg args[] = {
				{.isUBO = 0, .name = "current"},
				{.isUBO = 0, .name = "previous"}
			};
			shader_edgeDetermine = gl_shader_create((ivec4){arrayLength(vs), arrayLength(fs), 0, arrayLength(args)}, vs, fs, NULL, args);
			if (!shader_edgeDetermine) {
				fputs("Cannot load shader: Edge determine\n", stderr);
				goto label_exit;
			}
			shader_edgeDetermine_paramCurrent = args[0].id;
			shader_edgeDetermine_paramPrevious = args[1].id;
		}

		/* Create program: Edge fix */ {
			char cfgTolerance[100];
			int cfgToleranceCurrent = EDGE_TOLERANCE_CURRENT / roadinfo.orthoPixelWidth;
			int cfgTolerancePrevious = EDGE_TOLERANCE_PREVIOUS / roadinfo.orthoPixelWidth;
			sprintf(cfgTolerance, "const ivec4 tolerance = ivec4(%d, %d, %d, %d);"NL, cfgToleranceCurrent, cfgTolerancePrevious, 0, 0);
			fputs(cfgTolerance, stderr);

			gl_shaderSrc vs[] = {
				{.isFile = 0, .src = glShaderHeader},
				{.isFile = 0, .src = glShaderLine0},
				{.isFile = 1, .src = "shader/focusRegion.vs.glsl"}
			};
			gl_shaderSrc fs[] = {
				{.isFile = 0, .src = glShaderHeader},
				{.isFile = 0, .src = cfgTolerance},
				{.isFile = 0, .src = glShaderLine0},
				{.isFile = 1, .src = "shader/edgeFix.fs.glsl"}
			};
			gl_shaderArg args[] = {
				{.isUBO = 0, .name = "edgemap"}
			};
			shader_edgeFix = gl_shader_create((ivec4){arrayLength(vs), arrayLength(fs), 0, arrayLength(args)}, vs, fs, NULL, args);
			if (!shader_edgeFix) {
				fputs("Cannot load shader: Edge determine\n", stderr);
				goto label_exit;
			}
			shader_edgeFix_paramEdgemap = args[0].id;
		}
		
		/* Create program: Distance measure */ {
			char cfgBias[100];
			float cfgBiasOffset = 0.0f;
			float cfgBiasFirstOrder = fps * 3.6f;
			sprintf(cfgBias, "const vec4 bias = vec4(%.3f, %.3f, %.3f, %.3f);"NL, cfgBiasOffset, cfgBiasFirstOrder, 0.0f, 0.0f);

			gl_shaderSrc vs[] = {
				{.isFile = 0, .src = glShaderHeader},
				{.isFile = 0, .src = glShaderLine0},
				{.isFile = 1, .src = "shader/focusRegion.vs.glsl"}
			};
			gl_shaderSrc fs[] = {
				{.isFile = 0, .src = glShaderHeader},
				{.isFile = 0, .src = cfgBias},
				{.isFile = 0, .src = glShaderLine0},
				{.isFile = 1, .src = "shader/distanceMeasure.fs.glsl"}
			};
			gl_shaderArg args[] = {
				{.isUBO = 0, .name = "edgemap"},
				{.isUBO = 0, .name = "roadmapT1"},
				{.isUBO = 0, .name = "roadmapT2"}
			};
			shader_distanceMeasure = gl_shader_create((ivec4){arrayLength(vs), arrayLength(fs), 0, arrayLength(args)}, vs, fs, NULL, args);
			if (!shader_distanceMeasure) {
				fputs("Cannot load shader: Distance measure\n", stderr);
				goto label_exit;
			}
			shader_distanceMeasure_paramEdgemap = args[0].id;
			shader_distanceMeasure_paramRoadmapT1 = args[1].id;
			shader_distanceMeasure_paramRoadmapT2 = args[2].id;
		}

		/* Create program: Speed sample */ { //This program can be combined with speedometer if geometry shader is supported
			const char* cfg =
				"const int speedPoolSize = 4;"NL
				"const float speedSensitive = 0.05;"NL
				"const int speedDisplayCutoff = 0;"NL;

			gl_shaderSrc vs[] = {
				{.isFile = 0, .src = glShaderHeader},
				{.isFile = 0, .src = cfg},
				{.isFile = 0, .src = glShaderLine0},
				{.isFile = 1, .src = "shader/speedSample.vs.glsl"}
			};
			gl_shaderSrc fs[] = {
				{.isFile = 0, .src = glShaderHeader},
				{.isFile = 0, .src = glShaderLine0},
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
				{.isFile = 0, .src = glShaderHeader},
				{.isFile = 0, .src = glShaderLine0},
				{.isFile = 1, .src = "shader/speedometer.vs.glsl"}
			};
			gl_shaderSrc fs[] = {
				{.isFile = 0, .src = glShaderHeader},
				{.isFile = 0, .src = glShaderLine0},
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

#endif

		/* Create program: Final display */ {
			gl_programSrc src[] = {
				{gl_programSrcType_vertex,	gl_programSrcLoc_mem,	"#version 310 es\nprecision mediump float;\n"},
				{gl_programSrcType_vertex,	gl_programSrcLoc_file,	"shader/final.vs.glsl"},
				{gl_programSrcType_fragment,	gl_programSrcLoc_mem,	"#version 310 es\nprecision lowp float;\n"}, //Just blend, lowp is good enough
				{gl_programSrcType_fragment,	gl_programSrcLoc_mem,	"#define MIX_LEVEL 0.1\n"},
				{gl_programSrcType_fragment,	gl_programSrcLoc_file,	"shader/final.fs.glsl"},
				{.str = NULL}
			};
			gl_programArg arg[] = {
				{gl_programArgType_normal,	"orginalTexture"},
				{gl_programArgType_normal,	"processedTexture"},
				{.name = NULL}
			};
			program_final.pid = gl_program_create(src, arg);
			program_final.orginal = arg[0].id;
			program_final.result = arg[1].id;
			if (!checkShaderProgram("Final", program_final.pid, src, arg, stderr))
				goto label_exit;
		}
	}

	void ISR_SIGINT() {
		gl_close(1);
	}
	signal(SIGINT, ISR_SIGINT);

	/* Main process loop here */ {
		uint64_t timestamp = 0;
		fputs("Program ready!\n", stdout);
		while(!gl_close(-1)) {
			const int robin2[] = {1, 0, 1};
			const int* history2 = &robin2[ 1 - ((unsigned int)frameCnt & (unsigned int)0b1) ];
			const int robin4[] = {3, 2, 1, 0, 3, 2, 1};
			const int* history4 = &robin4[ 3 - ((unsigned int)frameCnt & (unsigned int)0b11) ];
			const int robin8[] = {7, 6, 5, 4, 3, 2, 1, 0, 7, 6, 5, 4, 3, 2, 1};
			const int* history8 = &robin8[ 7 - ((unsigned int)frameCnt & (unsigned int)0b111) ];
			fputc('\r', stdout); //Clear output

			int sizeWin[2], sizeFB[2];
			double cursorPosWin[2];
			int cursorPosFB[2]; //Window size and framebuffer size may differ if DPI is not 1, data
			int cursorPosData[2]; //Cursor position relative to data may differ from it to framebuffer if window is not displayed in data size
			gl_drawStart(cursorPosWin, sizeWin, sizeFB);
			cursorPosFB[0] = cursorPosWin[0] * sizeFB[0] / sizeWin[0];
			cursorPosFB[1] = cursorPosWin[1] * sizeFB[1] / sizeWin[1];
			cursorPosData[0] = cursorPosFB[0] * sizeData[0] / sizeFB[0];
			cursorPosData[1] = cursorPosFB[1] * sizeData[1] / sizeFB[1];

			if (!inBox(cursorPosData[0], cursorPosData[1], 0, sizeData[0], 0, sizeData[1], -1)) {
			
				/* Asking the read thread to load next frame while the main thread processing current frame */ {
					void* addr;
					#ifdef USE_PBO_UPLOAD
						gl_pixelBuffer_updateToTexture(&pbo[ (frameCnt+0)&1 ], &texture_orginalBuffer); //Current frame PBO (index = +0) <-- Data already in GPU, this operation is fast
						addr = gl_pixelBuffer_updateStart(&pbo[ (frameCnt+1)&1 ], sizeData[0] * sizeData[1] * 4); //Next frame PBO (index = +1)
					#else
						gl_texture_update(&texture_orginalBuffer, rawData[ (frameCnt+0)&1 ], zeros, sizeData);
						addr = rawData[ (frameCnt+1)&1 ];
					#endif
					th_reader_newAddress(addr);
				}

				uint64_t timestampRenderStart = nanotime();
				gl_setViewport(zeros, sizeData);

				/* Debug use ONLY: Check roadmap */ /*{
					gl_program_use(&program_roadmapCheck.pid);
					gl_texture_bind(&texture_roadmap1, program_roadmapCheck.roadmapT1, 0);
					gl_texture_bind(&texture_roadmap2, program_roadmapCheck.roadmapT2, 1);
					gl_program_setParam(program_roadmapCheck.cfgI1, 4, gl_datatype_int, (const int[4]){program_roadmapCheck_modeShowPerspGrid, 0, 0, 0});
					gl_program_setParam(program_roadmapCheck.cfgF1, 4, gl_datatype_float, (const float[4]){1.0, 2.0, 1.0, 1.0});
					gl_frameBuffer_bind(&fb_stageB.fbo, 1);
					gl_mesh_draw(&mesh_final);
				}*/

				/* Blur the raw to remove noise */ {
					gl_program_use(&program_blurFilter.pid);
					gl_texture_bind(&texture_orginalBuffer, program_blurFilter.src, 0);
					gl_frameBuffer_bind(&fb_raw[ history2[0] ].fbo, 1);
					gl_mesh_draw(&mesh_persp);
				}

#if 6 == 6

				/* Finding changing to detect moving object*/ {
					gl_program_use(&program_changingSensor.pid);
					gl_texture_bind(&fb_raw[ history2[0] ].tex, program_changingSensor.current, 0);
					gl_texture_bind(&fb_raw[ history2[1] ].tex, program_changingSensor.previous, 1);
					gl_frameBuffer_bind(&fb_stageA.fbo, 1);
					gl_mesh_draw(&mesh_persp);
				}

				/* Fix object */ {
					gl_program_use(&program_objectFix[0].pid);
					gl_texture_bind(&fb_stageA.tex, program_objectFix[0].src, 0);
					gl_texture_bind(&texture_roadmap1, program_objectFix[0].roadmapT1, 1);
					gl_frameBuffer_bind(&fb_stageB.fbo, 1);
					gl_mesh_draw(&mesh_persp);

					gl_program_use(&program_objectFix[1].pid);
					gl_texture_bind(&fb_stageB.tex, program_objectFix[1].src, 0);
					gl_texture_bind(&texture_roadmap1, program_objectFix[1].roadmapT1, 1);
					gl_frameBuffer_bind(&fb_stageA.fbo, 1);
					gl_mesh_draw(&mesh_persp);
				}

				/* Project from perspective to orthographic */ {
					gl_program_use(&program_projectP2O.pid);
					gl_texture_bind(&fb_stageA.tex, program_projectP2O.src, 0);
					gl_texture_bind(&texture_roadmap2, program_projectP2O.roadmapT2, 1);
					gl_frameBuffer_bind(&fb_stageB.fbo, 1);
					gl_mesh_draw(&mesh_ortho);
				}

				/* Refine edge, thinning the thick edge */ {
					gl_program_use(&program_edgeRefine.pid);
					gl_texture_bind(&fb_stageB.tex, program_edgeRefine.src, 0);
					gl_frameBuffer_bind(&fb_object[ history4[0] ].fbo, 1);
					gl_mesh_draw(&mesh_ortho);
				}

				/* Measure the distance of edge moving between current frame and previous frame */ {
					gl_program_use(&program_measure.pid);
					gl_texture_bind(&fb_object[ history4[0] ].tex, program_measure.current, 0);
					gl_texture_bind(&fb_object[ history4[INPUT_INTERLEAVE] ].tex, program_measure.previous, 1);
					gl_texture_bind(&texture_roadmap1, program_measure.roadmapT1, 2);
					gl_texture_bind(&texture_roadmap2, program_measure.roadmapT2, 3);
					gl_frameBuffer_bind(&fb_stageA.fbo, 1);
					gl_mesh_draw(&mesh_ortho);
				}

#endif
#if 3 == 4

				/* Measure the distance of edge moving between current frame and previous frame */ {
					gl_shader_use(&shader_measure);
					gl_texture_bind(&framebuffer_edge[rri].texture, shader_measure_paramCurrent, 0);
					gl_texture_bind(&framebuffer_edge[rrj].texture, shader_measure_paramPrevious, 1);
					gl_texture_bind(&texture_roadmap1, shader_measure_paramRoadmapT1, 2);
					gl_texture_bind(&texture_roadmap2, shader_measure_paramRoadmapT2, 3);
					gl_frameBuffer_bind(&framebuffer_stageA, vSize, 1);
					gl_mesh_draw(&mesh_ortho);
				}

				/* Project from  orthographic to perspective */ {
					gl_shader_use(&shader_projectO2P);
					gl_texture_bind(&framebuffer_stageA.texture, shader_projectP2O_paramSrc, 0);
					gl_texture_bind(&texture_roadmap2, shader_projectP2O_paramRoadmapT2, 1);
					gl_frameBuffer_bind(&framebuffer_speed, vSize, 1);
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
					gl_frameBuffer_bind(&framebuffer_display, vSize, 1);
					gl_mesh_draw(&mesh_speedometer);
				}
#endif

				#define RESULT fb_stageA
//				#define RESULT fb_raw[ history2[0] ]
//				#define RESULT fb_object[ history4[0] ]
//				#define RESULT (&framebuffer_edge[ history2[0] ])
//				#define RESULT (&framebuffer_speed)
//				#define RESULT (&framebuffer_display)

#if 1 == 2

				/* Apply edge filter */ {
					gl_shader_use(&shader_edgeFilter);
					gl_texture_bind(&framebuffer_raw[rri].texture, shader_edgeFilter_paramSrc, 0);
					gl_frameBuffer_bind(&framebuffer_stageA, vSize, 1);
					gl_mesh_draw(&mesh_persp);
				}

				/* Remove fixed objects by finding changing pixels between two frames (pixel change means movement) */ {
					gl_shader_use(&shader_changingSensor);
					gl_texture_bind(&framebuffer_edge[rri].texture, shader_changingSensor_paramCurrent, 0);
					gl_texture_bind(&framebuffer_edge[rrj].texture, shader_changingSensor_paramPrevious, 1);
					gl_frameBuffer_bind(&framebuffer_move[rri], vSize, 1);
					gl_mesh_draw(&mesh_ortho);
				}
				
				/* Determine edge */ {
					gl_shader_use(&shader_edgeDetermine);
					gl_texture_bind(&framebuffer_move[rri].texture, shader_edgeDetermine_paramCurrent, 0);
					gl_texture_bind(&framebuffer_move[rrj].texture, shader_edgeDetermine_paramPrevious, 1);
					gl_frameBuffer_bind(&framebuffer_stageA, vSize, 1);
					gl_mesh_draw(&mesh_ortho);
				}

				/* Fix the tear in edgemap */ {
					gl_shader_use(&shader_edgeFix);
					gl_texture_bind(&framebuffer_stageA.texture, shader_edgeFix_paramEdgemap, 0);
					gl_frameBuffer_bind(&framebuffer_stageB, vSize, 1);
					gl_mesh_draw(&mesh_ortho);
				}

				/* Measure the distance of edge moving between current frame and previous frame */ {
					gl_shader_use(&shader_distanceMeasure);
					gl_texture_bind(&framebuffer_stageB.texture, shader_distanceMeasure_paramEdgemap, 0);
					gl_texture_bind(&texture_roadmap1, shader_distanceMeasure_paramRoadmapT1, 1);
					gl_texture_bind(&texture_roadmap2, shader_distanceMeasure_paramRoadmapT2, 2);
					gl_frameBuffer_bind(&framebuffer_stageA, vSize, 1);
					gl_mesh_draw(&mesh_persp);
				}

#endif


				/* Draw final result on screen */ {
					gl_setViewport(zeros, sizeFB);
					gl_program_use(&program_final.pid);
					gl_texture_bind(&texture_orginalBuffer, program_final.orginal, 0);
					gl_texture_bind(&RESULT.tex, program_final.result, 1);
					gl_frameBuffer_bind(NULL, 0);
					gl_mesh_draw(&mesh_final);
				}

			
				gl_synch barrier = gl_synchSet();
				if (gl_synchWait(barrier, GL_SYNCH_TIMEOUT) == gl_synch_timeout) { //timeout = 5e9 ns = 5s
					fputs("Render loop fatal stall\n", stderr);
					goto label_exit;
				}
				#ifdef DEBUG_THREADSPEED
					debug_threadSpeed = 'M'; //Not critical, no need to use mutex
				#endif
				gl_synchDelete(barrier);

				char title[100];
				sprintf(title, "Viewer - frame %u", frameCnt);
				gl_drawEnd(title);

				uint64_t timestampRenderEnd = nanotime();
				
				th_reader_waitReader(); //Wait reader thread finish uploading frame data
				#ifdef USE_PBO_UPLOAD
					gl_pixelBuffer_updateFinish();
				#endif

				#ifdef DEBUG_THREADSPEED
					fputc(debug_threadSpeed, stdout);
					fputs(" - ", stdout);
				#endif
				fprintf(stdout, "Frame %u takes %.3lfms/%.3lfms (in-frame/inter-frame)", frameCnt, (timestampRenderEnd - timestampRenderStart) / (double)1e6, (timestampRenderEnd - timestamp) / (double)1e6);
				timestamp = timestampRenderEnd;
				fflush(stdout);

				#if FRAME_DELAY
					if (frameCnt > FRAME_DEBUGSKIPSECOND * fps) usleep(FRAME_DELAY);
				#endif

				frameCnt++;
			} else {
				char title[200];
				float color[4];
				gl_frameBuffer_download(&RESULT.fbo, color, gl_texformat_RGBA32F, 0, cursorPosData, (const uint[2]){1,1});
				sprintf(title, "Viewer - frame %u, Cursor=(%d,%d), result=(%.3f|%.3f|%.3f|%.3f)", frameCnt, cursorPosData[0], cursorPosData[1], color[0], color[1], color[2], color[3]);
				gl_drawEnd(title);
				usleep(25000);
			}
			
			
		}
	}

	/* Free all resources, house keeping */
	status = EXIT_SUCCESS;
label_exit:

	/*gl_program_delete(&shader_speedometer);
	gl_program_delete(&shader_distanceMeasure);
	gl_program_delete(&shader_edgeDetermine);
	gl_program_delete(&shader_projectO2P);
	gl_program_delete(&shader_projectP2O);
	gl_program_delete(&shader_edgeRefine);
	gl_program_delete(&shader_changingSensor);
	gl_program_delete(&shader_edgeFilter);
	gl_program_delete(&shader_blurFilter);
	gl_program_delete(&shader_roadmapCheck);*/

	/*gl_frameBuffer_delete(&framebuffer_stageB);
	gl_frameBuffer_delete(&framebuffer_stageA);
	gl_frameBuffer_delete(&framebuffer_display);
	gl_frameBuffer_delete(&framebuffer_speed);
	gl_frameBuffer_delete(&framebuffer_move[1]);
	gl_frameBuffer_delete(&framebuffer_move[0]);
	gl_frameBuffer_delete(&framebuffer_edge[1]);
	gl_frameBuffer_delete(&framebuffer_edge[0]);
	gl_frameBuffer_delete(&framebuffer_raw[1]);
	gl_frameBuffer_delete(&framebuffer_raw[0]);*/

	gl_texture_delete(&texture_speedometer);
	gl_mesh_delete(&mesh_speedometer);
	gl_mesh_delete(&mesh_speedsample);

	gl_texture_delete(&texture_roadmap2);
	gl_texture_delete(&texture_roadmap1);
	gl_mesh_delete(&mesh_ortho);
	gl_mesh_delete(&mesh_persp);

	th_reader_destroy();
	gl_texture_delete(&texture_orginalBuffer);
	#ifdef USE_PBO_UPLOAD
		gl_pixelBuffer_delete(&pbo[1]);
		gl_pixelBuffer_delete(&pbo[0]);
	#else
		free(rawData[0]);
		free(rawData[1]);
	#endif

	gl_destroy();

	info("\n%u frames displayed.\n", frameCnt);
	return status;
}

int checkShaderProgram(const char* name, const gl_program program, const gl_programSrc* srcs, const gl_programArg* args, const FILE* stream) {
	if (!program) {
		fprintf((FILE*)stream, "Fail to create program: %s\n", name);
		for (const gl_programSrc* src = srcs; src->str; src++) {
			const char* str = src->str;
			unsigned int len;
			for (len = 0; len < 60; len++) {
				if ( str[len] == '\n' || str[len] == '\0' )
					break;
			}
			switch (src->loc) {
				case gl_programSrcLoc_mem:
					fprintf((FILE*)stream, "\t- Mem @ %p: %.*s\n", str, len, str);
					break;
				case gl_programSrcLoc_file:
					fprintf((FILE*)stream, "\t- File: %.*s\n", len, str);
					break;
				default:
					fputs("\t- Bad loc\n", (FILE*)stream);
			}
		}
		return 0;
	}
	for (const gl_programArg* arg = args; arg->name; arg++) {
		if (arg->id == -1) {
			fprintf((FILE*)stream, "Argument \"%s\" missing in shader code.\n", arg->name);
			return 0;
		}
	}
	return 1;
}