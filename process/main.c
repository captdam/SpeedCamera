#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>

#include "common.h"
#include "gl.h"
#include "roadmap.h"
#include "speedometer.h"
#include "th_reader.h"

/* Program config */
#define MAX_SPEED 200 //km/h
#define GL_SYNCH_TIMEOUT 5000000000LLU //For gl sync timeout

/* Debug time delay */
#define FRAME_DELAY (50 * 1000)
#define FRAME_DEBUGSKIPSECOND 0

/* Speed avg */
#define INTERLACE 3

/* Shader config */
#define SHADER_CHANGINGSENSOR_THRESHOLD "0.05" //minimum changing in RGB to pass test
#define SHADER_OBJECTFIX_STEP "2.0" //Search step, default (min) is 2.0, larger value skips some pixel but increase performance
#define SHADER_OBJECTFIX_HDISTANCE "1.0" //Object fix gap distance in meter, larger value decrease performance
#define SHADER_OBJECTFIX_VDISTANCE "1.0"
#define SHADER_EDGEREFINE_BOTTOMDENOISE "0.2" //Bottom and side clerance of edge in meter
#define SHADER_EDGEREFINE_SIDEDNOISE "0.6"
#define SHADER_SPEEDOMETER_CNT 32 //Max number of speedometer
#define SHADER_SPEEDOMETER_WIDTH 0.04 //Relative NTC
#define SHADER_SPEEDOMETER_HEIGHT 0.025
#define SHADER_FINAL_RAWLUMA "0.3" //Blend intensity of raw

/* Speedometer */
#define SPEEDOMETER_FILE "./textmap.data"

#define DEBUG_THREADSPEED
#ifdef DEBUG_THREADSPEED
volatile char debug_threadSpeed = ' '; //Which thread takes longer
#endif

/* Video data upload to GPU and processed data download to CPU */
//#define USE_PBO_UPLOAD //Not big gain: uploading is asynch op, driver will copy data to internal buffer tand then upload
#define USE_PBO_DOWNLOAD //Big gain: download is synch op 

#define info(format, ...) {fprintf(stderr, "Log:\t"format"\n" __VA_OPT__(,) __VA_ARGS__);} //Write log
#define error(format, ...) {fprintf(stderr, "Err:\t"format"\n" __VA_OPT__(,) __VA_ARGS__);} //Write error log

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

		if (sizeData[0] & (unsigned int)0b111 || sizeData[0] < 320 || sizeData[0] > 2048) {
			error("Bad width: Width must be multiple of 8, 320 <= width <= 2048");
			return status;
		}
		if (sizeData[1] & (unsigned int)0b111 || sizeData[1] < 240 || sizeData[1] > 2048) {
			error("Bad height: Height must be multiple of 8, 240 <= height <= 2048");
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
		gl_pbo pboUpload[2] = {GL_INIT_DEFAULT_PBO, GL_INIT_DEFAULT_PBO};
	#else
		void* rawData[2] = {NULL, NULL};
	#endif
	gl_tex texture_orginalBuffer[2] = {GL_INIT_DEFAULT_TEX, GL_INIT_DEFAULT_TEX}; //Front texture for using, back texture up updating

	//Roadinfo, a mesh to store focus region, and texture to store road-domain data
	gl_mesh mesh_persp = GL_INIT_DEFAULT_MESH;
	gl_mesh mesh_ortho = GL_INIT_DEFAULT_MESH;
	gl_tex texture_roadmap1 = GL_INIT_DEFAULT_TEX; //Geo coord in persp and ortho views
	gl_tex texture_roadmap2 = GL_INIT_DEFAULT_TEX; //Up and down search limit, P2O and O2P project lookup
	struct {
		float left, right, top, bottom;
	} road_focusRegionBox;
	struct {
		unsigned int width, height;
		roadmap_t1* map;
	} road_geoMap = {.map = NULL};

	//To display human readable text on screen
	gl_tex texture_speedometer = GL_INIT_DEFAULT_TEX; //Glyph
	float* instance_speedometer_data = NULL; //Speed data to be draw (sx, sy, speed)
	gl_instance instance_speedometer = GL_INIT_DEFAULT_INSTANCE;
	gl_mesh mesh_display = GL_INIT_DEFAULT_MESH;

	//Analysis and export data (CPU side)
	float (* speedData)[sizeData[0]][2] = NULL; //FBO download, height(sizeData[1]) * width(sizeData[0]) * Channel(2)
	#ifdef USE_PBO_DOWNLOAD
		gl_synch pboDownloadSynch = NULL;
		gl_pbo pboDownload = GL_INIT_DEFAULT_PBO;
	#endif
	typedef struct AnalysisObject {
		float rx, ry; //Road- and screen-domain coord of current frame
		unsigned int sx, sy;
		float speed; //Speed (avged by shader)
		unsigned int osy; //Old screen y-coord 
	} analysisObj;
	analysisObj* speedAnalysisObj = NULL;

	//Final display on screen
	gl_mesh mesh_final = GL_INIT_DEFAULT_MESH;

	//Work stages: To save intermediate results during the process
	typedef struct Framebuffer {
		gl_fbo fbo;
		gl_tex tex;
		gl_texformat format;
	} fb;
	#define DEFAULT_FB (fb){GL_INIT_DEFAULT_FBO, GL_INIT_DEFAULT_TEX, gl_texformat_RGBA32F}
	fb fb_raw[2] = { //Raw video data with minor pre-process
		{GL_INIT_DEFAULT_FBO, GL_INIT_DEFAULT_TEX, gl_texformat_RGBA8}, //Input video, RGBA8
		{GL_INIT_DEFAULT_FBO, GL_INIT_DEFAULT_TEX, gl_texformat_RGBA8}
	};
	fb fb_object[INTERLACE + 1] = { //Object detection of current and previous frames
		[0 ... INTERLACE] = {GL_INIT_DEFAULT_FBO, GL_INIT_DEFAULT_TEX, gl_texformat_RGBA8} //Enum < 256
	};
	fb fb_speed = { //Speed measure result, road-domain speed in km/h and screen-domain speed in px/frame
		GL_INIT_DEFAULT_FBO,
		GL_INIT_DEFAULT_TEX,
		gl_texformat_RG32F //Float range [0, 255], 16F is good enough, but use 32 for easy download in IEEE754 format, 16F in GPU may not follow standard format
	};
	fb fb_display = {GL_INIT_DEFAULT_FBO, GL_INIT_DEFAULT_TEX, gl_texformat_RGBA8}; //Display human-readable text, video, RGBA8
	fb fb_stageA = {GL_INIT_DEFAULT_FBO, GL_INIT_DEFAULT_TEX, gl_texformat_RGBA16F}; //General intermediate data, must be RGBA16F or RGBA32F
	fb fb_stageB = {GL_INIT_DEFAULT_FBO, GL_INIT_DEFAULT_TEX, gl_texformat_RGBA16F};

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

	//Program - Project perspective to orthographic and orthographic to perspective
	struct {
		gl_program pid;
		gl_param src;
		gl_param roadmapT2;
	} program_projectP2O = {.pid = GL_INIT_DEFAULT_PROGRAM};
	struct {
		gl_program pid;
		gl_param src;
		gl_param roadmapT2;
	} program_projectO2P = {.pid = GL_INIT_DEFAULT_PROGRAM};

	//Program - Blur filter and edge filter
	struct {
		gl_program pid;
		gl_param src;
	} program_blurFilter = {.pid = GL_INIT_DEFAULT_PROGRAM};
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
		gl_param roadmapT1;
	} program_edgeRefine = {.pid = GL_INIT_DEFAULT_PROGRAM};

	//Program - Measure
	struct {
		gl_program pid;
		gl_param current;
		gl_param hint;
		gl_param previous;
		gl_param roadmapT1;
		gl_param roadmapT2;
	} program_measure = {.pid = GL_INIT_DEFAULT_PROGRAM};

	//Program - Sample
	struct {
		gl_program pid;
		gl_param src;
	} program_sample = {.pid = GL_INIT_DEFAULT_PROGRAM};

	//Program - Display
	struct {
		gl_program pid;
		gl_param glyphmap;
	} program_display = {.pid = GL_INIT_DEFAULT_PROGRAM};

	//Program - Final
	struct {
		gl_program pid;
		gl_param orginal;
		gl_param result;
	} program_final = {.pid = GL_INIT_DEFAULT_PROGRAM};

	/* Init OpenGL and viewer window */ {
		info("Init openGL...");
		if (!gl_init((gl_config){
			.vMajor = 3,
			.vMinor = 1,
			.gles = 1,
			.winWidth = sizeData[0], .winHeight = sizeData[1],
			.winName = "Viewer"
		})) {
			error("Cannot init OpenGL");
			goto label_exit;
		}
	}

	/* Use a texture to store raw frame data & Start reader thread */ {
		info("Prepare video upload buffer...");
		#ifdef USE_PBO_UPLOAD
			pboUpload[0] = gl_pixelBuffer_create(sizeData[0] * sizeData[1] * 4, 0, gl_usage_stream); //Always use RGBA8 (good performance)
			pboUpload[1] = gl_pixelBuffer_create(sizeData[0] * sizeData[1] * 4, 0, gl_usage_stream);
			if ( !gl_pixelBuffer_check(&(pboUpload[0])) || !gl_pixelBuffer_check(&(pboUpload[1])) ) {
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

		texture_orginalBuffer[0] = gl_texture_create(gl_texformat_RGBA8, gl_textype_2d, sizeData);
		texture_orginalBuffer[1] = gl_texture_create(gl_texformat_RGBA8, gl_textype_2d, sizeData);
		if ( !gl_texture_check(&texture_orginalBuffer[0]) || !gl_texture_check(&texture_orginalBuffer[1]) ) {
			error("Fail to create texture buffer for orginal frame data storage");
			goto label_exit;
		}

		info("Init reader thread...");
		char* statue;
		int code;
		if (!th_reader_init(sizeData[0] * sizeData[1], color, &statue, &code)) {
			error("Fail to create reader thread: %s. Error code %d", statue, code);
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
		
		roadmap_genLimit(roadmap, MAX_SPEED / 3.6 / fps); //km/h to m/s to m/frame
		roadmap_header roadinfo = roadmap_getHeader(roadmap);
		const unsigned int sizeRoadmap[3] = {roadinfo.width, roadinfo.height, 0};

		/* Focus region */ {
			const gl_index_t attributes[] = {2, 0};
			float (* vertices)[2] = (float(*)[2])roadmap_getRoadPoints(roadmap);
			
			road_focusRegionBox.left = vertices[roadinfo.pCnt-4][0];
			road_focusRegionBox.right = vertices[roadinfo.pCnt-1][0];
			road_focusRegionBox.top = vertices[roadinfo.pCnt-4][1];
			road_focusRegionBox.bottom = vertices[roadinfo.pCnt-1][1];

			mesh_persp = gl_mesh_create(roadinfo.pCnt-4, 0, attributes, NULL, (gl_vertex_t*)vertices, NULL, NULL, gl_meshmode_triangleStrip, gl_usage_static);
			mesh_ortho = gl_mesh_create(4, 0, attributes, NULL, (gl_vertex_t*)(vertices + roadinfo.pCnt-4), NULL, NULL, gl_meshmode_triangleStrip, gl_usage_static);
			if ( !gl_mesh_check(&mesh_persp) || !gl_mesh_check(&mesh_ortho) ) {
				roadmap_destroy(roadmap);
				error("Fail to create mesh for roadmap - focus region");
				goto label_exit;
			}
		}
		
		/* Road data */ {
			texture_roadmap1 = gl_texture_create(gl_texformat_RGBA16F, gl_textype_2d, sizeRoadmap); //Mediump is good enough for road-domain geo locations
			texture_roadmap2 = gl_texture_create(gl_texformat_RGBA16F, gl_textype_2d, sizeRoadmap); //Mediump (1/1024) is OK for pixel indexing
			if ( !gl_texture_check(&texture_roadmap1) || !gl_texture_check(&texture_roadmap2) ) {
				roadmap_destroy(roadmap);
				error("Fail to create texture buffer for roadmap - road-domain data storage");
				goto label_exit;
			}

			road_geoMap.width = roadinfo.width;
			road_geoMap.height = roadinfo.height;
			if (!( road_geoMap.map = malloc(road_geoMap.width * road_geoMap.height * sizeof(roadmap_t1)) )) {
				roadmap_destroy(roadmap);
				error("Fail to allocate memory for roadmap storage for analysis");
				goto label_exit;
			}
			memcpy(road_geoMap.map, roadmap_getT1(roadmap), road_geoMap.width * road_geoMap.height * sizeof(roadmap_t1));

			gl_texture_update(&texture_roadmap1, roadmap_getT1(roadmap), zeros, sizeRoadmap);
			gl_texture_update(&texture_roadmap2, roadmap_getT2(roadmap), zeros, sizeRoadmap);
		}

		roadmap_destroy(roadmap); //Free roadmap memory after data uploading finished
	}

	/* Speedometer: speedometer glphy texture */ {
		info("Load resource - speedometer \"%s\"...", SPEEDOMETER_FILE);
		char* statue;
		Speedometer speedometer = speedometer_init(SPEEDOMETER_FILE, &statue);
		if (!speedometer) {
			error("Cannot load speedometer: %s", statue);
			goto label_exit;
		}
		speedometer_convert(speedometer);

		unsigned int glyphSize[3] = {0,0,256};
		uint32_t* glyph = speedometer_getGlyph(speedometer, glyphSize);
		texture_speedometer = gl_texture_create(gl_texformat_RGBA8, gl_textype_2dArray, glyphSize);
		if (!gl_texture_check(&texture_speedometer)) {
			speedometer_destroy(speedometer);
			error("Fail to create texture buffer for speedometer");
			goto label_exit;
		}
		gl_texture_update(&texture_speedometer, glyph, zeros, glyphSize);

		speedometer_destroy(speedometer); //Free speedometer memory after data uploading finished

		instance_speedometer_data = malloc(SHADER_SPEEDOMETER_CNT * 3 * sizeof(float));
		if (!instance_speedometer_data) {
			error("Fail to allocate memory for speedometer instance buffer");
			goto label_exit;
		}
		instance_speedometer = gl_instance_create(SHADER_SPEEDOMETER_CNT * 3 * sizeof(float), gl_usage_stream);
		if (!gl_instance_check(&instance_speedometer)) {
			error("Fail to create instance array for speedometer");
			goto label_exit;
		}
		mesh_display = gl_mesh_create(
			4, 0,
			(gl_index_t[]){4, 0},
			(gl_index_t[]){3, 0},
			(const gl_vertex_t[]){
				+SHADER_SPEEDOMETER_WIDTH, -SHADER_SPEEDOMETER_HEIGHT, 1.0f, 0.0f,
				+SHADER_SPEEDOMETER_WIDTH, +SHADER_SPEEDOMETER_HEIGHT, 1.0f, 1.0f,
				-SHADER_SPEEDOMETER_WIDTH, +SHADER_SPEEDOMETER_HEIGHT, 0.0f, 1.0f,
				-SHADER_SPEEDOMETER_WIDTH, -SHADER_SPEEDOMETER_HEIGHT, 0.0f, 0.0f
			},
			NULL,
			&instance_speedometer,
			gl_meshmode_triangleFan,
			gl_usage_static
		);
		if (!gl_mesh_check(&mesh_display)) {
			error("Fail to create mesh to store speedometer");
			goto label_exit;
		}
	}

	/* Create buffer for post process on CPU side */ {
		#ifdef USE_PBO_DOWNLOAD
			pboDownload = gl_pixelBuffer_create(sizeData[0] * sizeData[1] * 2 * sizeof(float), 1, gl_usage_stream);
			if (!gl_pixelBuffer_check(&pboDownload)) {
				error("Fail to create pixel buffer for speed downloading");
				goto label_exit;
			}
		#else
			speedData = malloc(sizeData[0] * sizeData[1] * 2 * sizeof(float)); //FBO dump
			if (!speedData) {
				error("Fail to create buffer to download speed framebuffer");
				goto label_exit;
			}
		#endif
		speedAnalysisObj = malloc(SHADER_SPEEDOMETER_CNT * sizeof(analysisObj));
		if (!speedAnalysisObj) {
			error("Fail to create buffer to store analysis objects");
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
		mesh_final = gl_mesh_create(4, 0, (const gl_index_t[]){2, 0}, NULL, vertices, NULL, NULL, gl_meshmode_triangleFan, gl_usage_static);
		if (!gl_mesh_check(&mesh_final)) {
			error("Fail to create mesh for final display");
			goto label_exit;
		}
	}

	/* Create work buffer */ {
		info("Create GPU buffers...");

		for (unsigned int i = 0; i < arrayLength(fb_raw); i++) {
			fb_raw[i].tex = gl_texture_create(fb_raw[i].format, gl_textype_2d, sizeData);
			if (!gl_texture_check(&fb_raw[i].tex)) {
				error("Fail to create texture to store raw video data (%u)", i);
				goto label_exit;
			}
			fb_raw[i].fbo = gl_frameBuffer_create(1, (const gl_tex[]){fb_raw[i].tex}, (const gl_fboattach[]){gl_fboattach_color0});
			if (!gl_frameBuffer_check(&fb_raw[i].fbo) ) {
				error("Fail to create FBO to store raw video data (%u)", i);
				goto label_exit;
			}
		}

		for (unsigned int i = 0; i < arrayLength(fb_object); i++) {
			fb_object[i].tex = gl_texture_create(fb_object[i].format, gl_textype_2d, sizeData);
			if (!gl_texture_check(&fb_object[i].tex)) {
				error("Fail to create texture to store object (%u)", i);
				goto label_exit;
			}
			fb_object[i].fbo = gl_frameBuffer_create(1, (const gl_tex[]){fb_object[i].tex}, (const gl_fboattach[]){gl_fboattach_color0});
			if (!gl_frameBuffer_check(&fb_object[i].fbo) ) {
				error("Fail to create FBO to store object (%u)\n", i);
				goto label_exit;
			}
		}

		fb_speed.tex = gl_texture_create(fb_speed.format, gl_textype_2d, sizeData);
		if (!gl_texture_check(&fb_speed.tex)) {
			error("Fail to create texture to store speed");
			goto label_exit;
		}
		fb_speed.fbo = gl_frameBuffer_create(1, (const gl_tex[]){fb_speed.tex}, (const gl_fboattach[]){gl_fboattach_color0});
		if (!gl_frameBuffer_check(&fb_speed.fbo) ) {
			error("Fail to create FBO to store speed");
			goto label_exit;
		}

		fb_display.tex = gl_texture_create(fb_display.format, gl_textype_2d, sizeData);
		if (!gl_texture_check(&fb_display.tex)) {
			error("Fail to create texture to store speed");
			goto label_exit;
		}
		fb_display.fbo = gl_frameBuffer_create(1, (const gl_tex[]){fb_display.tex}, (const gl_fboattach[]){gl_fboattach_color0});
		if (!gl_frameBuffer_check(&fb_display.fbo) ) {
			error("Fail to create FBO to store speed");
			goto label_exit;
		}

		fb_stageA.tex = gl_texture_create(fb_stageA.format, gl_textype_2d, sizeData); //Data
		fb_stageB.tex = gl_texture_create(fb_stageB.format, gl_textype_2d, sizeData); //Data
		if ( !gl_texture_check(&fb_stageA.tex) || !gl_texture_check(&fb_stageB.tex) ) {
			error("Fail to create texture to store moving edge");
			goto label_exit;
		}
		fb_stageA.fbo = gl_frameBuffer_create(1, (const gl_tex[]){fb_stageA.tex}, (const gl_fboattach[]){gl_fboattach_color0});
		fb_stageB.fbo = gl_frameBuffer_create(1, (const gl_tex[]){fb_stageB.tex}, (const gl_fboattach[]){gl_fboattach_color0});
		if ( !gl_frameBuffer_check(&fb_stageA.fbo) || !gl_frameBuffer_check(&fb_stageB.fbo) ) {
			error("Fail to create FBO to store moving edge");
			goto label_exit;
		}
	}

	/* Load shader programs */ {
		info("Load shaders...");
		gl_program_setCommonHeader("#version 310 es");
		#define NL "\n"

		/* Create program: Roadmap check */ {
			gl_programSrc src[] = {
				{gl_programSrcType_vertex,	gl_programSrcLoc_file,	"shader/focusRegion.vs.glsl"},
				{gl_programSrcType_fragment,	gl_programSrcLoc_mem,	"precision highp float;"},
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

			if (!( program_roadmapCheck.pid = gl_program_create(src, arg) )) {
				error("Fail to create shader program: Roadmap check");
				goto label_exit;
			}
			program_roadmapCheck.roadmapT1 = arg[0].id;
			program_roadmapCheck.roadmapT2 = arg[1].id;
			program_roadmapCheck.cfgI1 = arg[2].id;
			program_roadmapCheck.cfgF1 = arg[3].id;
		}

		/* Create program: Project P2O and P2O */ {
			const char* modeP2O = "#define P2O";
			const char* modeO2P = "#define O2P";

			gl_programSrc src[] = {
				{gl_programSrcType_vertex,	gl_programSrcLoc_file,	"shader/focusRegion.vs.glsl"},
				{gl_programSrcType_fragment,	gl_programSrcLoc_mem,	NULL},
				{gl_programSrcType_fragment,	gl_programSrcLoc_file,	"shader/projectPO.fs.glsl"},
				{.str = NULL}
			};
			gl_programArg arg[] = {
				{gl_programArgType_normal,	"src"},
				{gl_programArgType_normal,	"roadmapT2"},
				{.name = NULL}
			};

			src[1].str = modeP2O;
			if (!( program_projectP2O.pid = gl_program_create(src, arg) )) {
				error("Fail to create shader program: Project perspective to orthographic");
				goto label_exit;
			}
			program_projectP2O.src = arg[0].id;
			program_projectP2O.roadmapT2 = arg[1].id;

			src[1].str = modeO2P;
			if (!( program_projectO2P.pid = gl_program_create(src, arg) )) {
				error("Fail to create shader program: Project orthographic to perspective");
				goto label_exit;
			}
			program_projectO2P.src = arg[0].id;
			program_projectO2P.roadmapT2 = arg[1].id;
		}

		/* Create program: Blur and edge filter*/ {
			const char* blur =
				"const struct Mask {mediump float v; mediump ivec2 idx;} mask[] = Mask[]("NL
				"	Mask(1.0 / 16.0, ivec2(-1, -1)),	Mask(2.0 / 16.0, ivec2( 0, -1)),	Mask(1.0 / 16.0, ivec2(+1, -1)),"NL
				"	Mask(2.0 / 16.0, ivec2(-1,  0)),	Mask(4.0 / 16.0, ivec2( 0,  0)),	Mask(2.0 / 16.0, ivec2(+1,  0)),"NL
				"	Mask(1.0 / 16.0, ivec2(-1, +1)),	Mask(2.0 / 16.0, ivec2( 0, +1)),	Mask(1.0 / 16.0, ivec2(+1, +1))"NL
				");"NL
				/*"const struct Mask {mediump float v; mediump ivec2 idx;} mask[] = Mask[]("NL
				"	Mask(1.0 / 9.0, ivec2(-1, -1)),	Mask(1.0 / 9.0, ivec2( 0, -1)),	Mask(1.0 / 9.0, ivec2(+1, -1)),"NL
				"	Mask(1.0 / 9.0, ivec2(-1,  0)),	Mask(1.0 / 9.0, ivec2( 0,  0)),	Mask(1.0 / 9.0, ivec2(+1,  0)),"NL
				"	Mask(1.0 / 9.0, ivec2(-1, +1)),	Mask(1.0 / 9.0, ivec2( 0, +1)),	Mask(1.0 / 9.0, ivec2(+1, +1))"NL
				");"NL*/
			;
			const char* edge =
				"#define CLAMP vec4[2](vec4(vec3(0.0), 1.0), vec4(vec3(1.0), 1.0))"NL
				"const struct Mask {mediump float v; mediump ivec2 idx;} mask[] = Mask[]("NL
				"	Mask(-1.0, ivec2(0, -2)),"NL
				"	Mask(-2.0, ivec2(0, -1)),"NL
				"	Mask(+6.0, ivec2(0,  0)),"NL
				"	Mask(-2.0, ivec2(0, +1)),"NL
				"	Mask(-1.0, ivec2(0, +2))"NL
				");"NL
			;

			gl_programSrc src[] = {
				{gl_programSrcType_vertex,	gl_programSrcLoc_file,	"shader/focusRegion.vs.glsl"},
				{gl_programSrcType_fragment,	gl_programSrcLoc_mem,	NULL},
				{gl_programSrcType_fragment,	gl_programSrcLoc_file,	"shader/kernel.fs.glsl"},
				{.str = NULL}
			};
			gl_programArg arg[] = {
				{gl_programArgType_normal,	"src"},
				{.name = NULL}
			};

			src[1].str = blur;
			if (!( program_blurFilter.pid = gl_program_create(src, arg) )) {
				error("Fail to create shader program: Blur filter");
				goto label_exit;
			}
			program_blurFilter.src = arg[0].id;

			src[1].str = edge;
			if (!( program_edgeFilter.pid = gl_program_create(src, arg) )) {
				error("Fail to create shader program: Edge filter");
				goto label_exit;
			}
			program_edgeFilter.src = arg[0].id;
		}
		
		/* Create program: Changing sensor */ {
			const char* cfg =
				"#define BINARY "SHADER_CHANGINGSENSOR_THRESHOLD NL
			//	"#define CLAMP float[2](0.0, 1.0)"NL
			;

			gl_programSrc src[] = {
				{gl_programSrcType_vertex,	gl_programSrcLoc_file,	"shader/focusRegion.vs.glsl"},
				{gl_programSrcType_fragment,	gl_programSrcLoc_mem,	cfg},
				{gl_programSrcType_fragment,	gl_programSrcLoc_file,	"shader/changingSensor.fs.glsl"},
				{.str = NULL}
			};
			gl_programArg arg[] = {
				{gl_programArgType_normal,	"current"},
				{gl_programArgType_normal,	"previous"},
				{.name = NULL}
			};

			if (!( program_changingSensor.pid = gl_program_create(src, arg) )) {
				error("Fail to create shader program: Changing sensor");
				goto label_exit;
			}
			program_changingSensor.current = arg[0].id;
			program_changingSensor.previous = arg[1].id;
		}

		/* Create program: Object fix (2 stages) */ {
			char cfg[200];
			sprintf(cfg,
				"#define STEP "SHADER_OBJECTFIX_STEP NL"#define EDGE vec4(%.4f, %.4f, %.4f, %.4f)"NL,
				road_focusRegionBox.left, road_focusRegionBox.right,
				road_focusRegionBox.top, road_focusRegionBox.bottom
			);

			char* horizontal =
				"#define HORIZONTAL"NL
				"#define SEARCH_DISTANCE "SHADER_OBJECTFIX_HDISTANCE NL
			;
			char* vertical =
				"#define VERTICAL"NL
				"#define SEARCH_DISTANCE "SHADER_OBJECTFIX_VDISTANCE NL
			;

			gl_programSrc src[] = {
				{gl_programSrcType_vertex,	gl_programSrcLoc_file,	"shader/focusRegion.vs.glsl"},
				{gl_programSrcType_fragment,	gl_programSrcLoc_mem,	cfg},
				{gl_programSrcType_fragment,	gl_programSrcLoc_mem,	NULL},
				{gl_programSrcType_fragment,	gl_programSrcLoc_file,	"shader/objectFix.fs.glsl"},
				{.str = NULL}
			};
			gl_programArg arg[] = {
				{gl_programArgType_normal,	"src"},
				{gl_programArgType_normal,	"roadmapT1"},
				{.name = NULL}
			};

			src[2].str = horizontal;
			if (!( program_objectFix[0].pid = gl_program_create(src, arg) )) {
				error("Fail to create shader program: Object fix - Stage 1 horizontal");
				goto label_exit;
			}
			program_objectFix[0].src = arg[0].id;
			program_objectFix[0].roadmapT1 = arg[1].id;

			src[2].str = vertical;
			if (!( program_objectFix[1].pid = gl_program_create(src, arg) )) {
				error("Fail to create shader program: Object fix - Stage 2 vertical");
				goto label_exit;
			}
			program_objectFix[1].src = arg[0].id;
			program_objectFix[1].roadmapT1 = arg[1].id;
		}

		/* Create program: Edge refine */ {
			const char* cfg =
				"#define DENOISE_BOTTOM "SHADER_EDGEREFINE_BOTTOMDENOISE NL
				"#define DENOISE_SIDE "SHADER_EDGEREFINE_SIDEDNOISE NL
			;

			gl_programSrc src[] = {
				{gl_programSrcType_vertex,	gl_programSrcLoc_file,	"shader/focusRegion.vs.glsl"},
				{gl_programSrcType_fragment,	gl_programSrcLoc_mem,	cfg},
				{gl_programSrcType_fragment,	gl_programSrcLoc_file,	"shader/edgeRefine.fs.glsl"},
				{.str = NULL}
			};
			gl_programArg arg[] = {
				{gl_programArgType_normal,	"src"},
				{gl_programArgType_normal,	"roadmapT1"},
				{.name = NULL}
			};

			if (!( program_edgeRefine.pid = gl_program_create(src, arg) )) {
				error("Fail to create shader program: Edge refine");
				goto label_exit;
			}
			program_edgeRefine.src = arg[0].id;
			program_edgeRefine.roadmapT1 = arg[1].id;
		}

		/* Create program: Measure */ {
			char cfg[200];
			float cfgBias = fps * 3.6f / INTERLACE;
			sprintf(cfg, "#define BIAS %.4f"NL"#define LIMIT_UP %d"NL"#define LIMIT_DOWN %d"NL, cfgBias, (int)(road_focusRegionBox.top * sizeData[1]), (int)(road_focusRegionBox.bottom * sizeData[1]));

			gl_programSrc src[] = {
				{gl_programSrcType_vertex,	gl_programSrcLoc_file,	"shader/focusRegion.vs.glsl"},
				{gl_programSrcType_fragment,	gl_programSrcLoc_mem,	cfg},
				{gl_programSrcType_fragment,	gl_programSrcLoc_file,	"shader/measure.fs.glsl"},
				{.str = NULL}
			};
			gl_programArg arg[] = {
				{gl_programArgType_normal,	"current"},
				{gl_programArgType_normal,	"hint"},
				{gl_programArgType_normal,	"previous"},
				{gl_programArgType_normal,	"roadmapT1"},
				{gl_programArgType_normal,	"roadmapT2"},
				{.name = NULL}
			};

			if (!( program_measure.pid = gl_program_create(src, arg) )) {
				error("Fail to create shader program: Measure");
				goto label_exit;
			}
			program_measure.current = arg[0].id;
			program_measure.hint = arg[1].id;
			program_measure.previous = arg[2].id;
			program_measure.roadmapT1 = arg[3].id;
			program_measure.roadmapT2 = arg[4].id;
		}

		/* Create program: Sample */ {
			const char* cfg = "#define MIN_SAMPLE_SIZE 2.0";

			gl_programSrc src[] = {
				{gl_programSrcType_vertex,	gl_programSrcLoc_file,	"shader/focusRegion.vs.glsl"},
				{gl_programSrcType_fragment,	gl_programSrcLoc_mem,	cfg},
				{gl_programSrcType_fragment,	gl_programSrcLoc_file,	"shader/sample.fs.glsl"},
				{.str = NULL}
			};
			gl_programArg arg[] = {
				{gl_programArgType_normal,	"src"},
				{.name = NULL}
			};

			if (!( program_sample.pid = gl_program_create(src, arg) )) {
				error("Fail to create shader program: Sample");
				goto label_exit;
			}
			program_sample.src = arg[0].id;
		}

		/* Create program: Display */ {
			gl_programSrc src[] = {
				{gl_programSrcType_vertex,	gl_programSrcLoc_file,	"shader/display.vs.glsl"},
				{gl_programSrcType_fragment,	gl_programSrcLoc_file,	"shader/display.fs.glsl"},
				{.str = NULL}
			};
			gl_programArg arg[] = {
				{gl_programArgType_normal,	"glyphmap"},
				{.name = NULL}
			};

			if (!( program_display.pid = gl_program_create(src, arg) )) {
				error("Fail to create shader program: Display");
				goto label_exit;
			}
			program_display.glyphmap = arg[0].id;
		}

		/* Create program: Final display */ {
			const char* cfg = "#define RAW_LUMA "SHADER_FINAL_RAWLUMA NL;

			gl_programSrc src[] = {
				{gl_programSrcType_vertex,	gl_programSrcLoc_file,	"shader/final.vs.glsl"},
				{gl_programSrcType_fragment,	gl_programSrcLoc_mem,	cfg},
				{gl_programSrcType_fragment,	gl_programSrcLoc_file,	"shader/final.fs.glsl"},
				{.str = NULL}
			};
			gl_programArg arg[] = {
				{gl_programArgType_normal,	"orginalTexture"},
				{gl_programArgType_normal,	"processedTexture"},
				{.name = NULL}
			};

			if (!( program_final.pid = gl_program_create(src, arg) )) {
				error("Fail to create shader program: Final");
				goto label_exit;
			}
			program_final.orginal = arg[0].id;
			program_final.result = arg[1].id;
		}
	}

	void ISR_SIGINT() {
		info("INT");
		gl_close(1);
	}
	signal(SIGINT, ISR_SIGINT);
	
	#ifdef VERBOSE_TIME
		uint64_t timestamp = 0;
	#endif
	info("Program ready!");
	fprintf(stdout, "R %u*%u : I %u\n", sizeData[0], sizeData[1], INTERLACE);
	
	/* Main process loop here */
	while(!gl_close(-1)) {
		gl_drawStart();
		char winTitle[200];

		const unsigned int current = (unsigned int)frameCnt & (unsigned int)0b1 ? 1 : 0; //Front
		const unsigned int previous = 1 - current; //Back
		const unsigned int current_obj = frameCnt % arrayLength(fb_object);
		const unsigned int hint_obj = (frameCnt + arrayLength(fb_object) - 1) % arrayLength(fb_object);
		const unsigned int previous_obj = (frameCnt + arrayLength(fb_object) - INTERLACE) % arrayLength(fb_object);

		gl_winsizeNcursor winsizeNcursor = gl_getWinsizeCursor();
		int cursorPosData[2] = {winsizeNcursor.curPos[0] * sizeData[0], winsizeNcursor.curPos[1] * sizeData[1]};

		if (!inBox(cursorPosData[0], cursorPosData[1], 0, sizeData[0], 0, sizeData[1], -1)) {
		
			/* Asking the read thread to upload next frame while the main thread processing current frame */ {
				void* addr;
				#ifdef USE_PBO_UPLOAD
					gl_pixelBuffer_updateToTexture(&pboUpload[current], &texture_orginalBuffer[previous]);
					addr = gl_pixelBuffer_updateStart(&pboUpload[previous], sizeData[0] * sizeData[1] * 4);
				#else
					gl_texture_update(&texture_orginalBuffer[previous], rawData[current], zeros, sizeData);
					addr = rawData[previous];
				#endif
				gl_rsync(); //Request the GL driver start the queue
				th_reader_start(addr);
				/* Note: 3-stage uploading scheme. 
					* Process front texture while OpenGL DMA front buffer to back texture. 
					* OpenGL DMA front buffer while read thread read video into back buffer. 
					*/
			}

			#ifdef VERBOSE_TIME
				uint64_t timestampRenderStart = nanotime();
			#endif

			gl_setViewport(zeros, sizeData);

			/* Blur the raw to remove noise */ {
				gl_program_use(&program_blurFilter.pid);
				gl_texture_bind(&texture_orginalBuffer[current], program_blurFilter.src, 0);
				gl_frameBuffer_bind(&fb_raw[current].fbo, 0);
				gl_mesh_draw(&mesh_final, 0, 0);
				//Although we only need mesh_persp, we use mesh_final here to store the whole scene. So we can use previous frame when display speedometer (speedometer has one frame latency)
				//Do not use texture_orginalBuffer[previous], it is actural next frame, updated in background
			}

//#define ROADMAP_CHECK program_roadmapCheck_modeShowPerspGrid //program_roadmapCheck_modeshow*
#ifdef ROADMAP_CHECK
			/* Debug use ONLY: Check roadmap */ {
				gl_program_use(&program_roadmapCheck.pid);
				gl_texture_bind(&texture_roadmap1, program_roadmapCheck.roadmapT1, 0);
				gl_texture_bind(&texture_roadmap2, program_roadmapCheck.roadmapT2, 1);
				gl_program_setParam(program_roadmapCheck.cfgI1, 4, gl_datatype_int, (const int[4]){ROADMAP_CHECK, 0, 0, 0});
				gl_program_setParam(program_roadmapCheck.cfgF1, 4, gl_datatype_float, (const float[4]){5.0, 5.0, 1.0, 1.0});
				gl_frameBuffer_bind(&fb_stageA.fbo, 1);
				gl_mesh_draw(&mesh_final, 0, 0);
			}
			#define RESULT fb_stageA
#else

			/* Project from perspective to orthographic */ /*{
				gl_program_use(&program_projectP2O.pid);
				gl_texture_bind(&fb_raw[current].tex, program_projectP2O.src, 0);
				gl_texture_bind(&texture_roadmap2, program_projectP2O.roadmapT2, 1);
				gl_frameBuffer_bind(&fb_stageB.fbo, 1);
				gl_mesh_draw(&mesh_ortho, 0, 0);
			}*/

			/* Finding changing to detect moving object*/ {
				gl_program_use(&program_changingSensor.pid);
				gl_texture_bind(&fb_raw[current].tex, program_changingSensor.current, 0);
				gl_texture_bind(&fb_raw[previous].tex, program_changingSensor.previous, 1);
				gl_frameBuffer_bind(&fb_stageA.fbo, gl_frameBuffer_clearColor);
				gl_mesh_draw(&mesh_persp, 0, 0);
			}

			/* Fix object */ {
				gl_program_use(&program_objectFix[0].pid);
				gl_texture_bind(&fb_stageA.tex, program_objectFix[0].src, 0);
				gl_texture_bind(&texture_roadmap1, program_objectFix[0].roadmapT1, 1);
				gl_frameBuffer_bind(&fb_stageB.fbo, gl_frameBuffer_clearColor);
				gl_mesh_draw(&mesh_persp, 0, 0);

				gl_program_use(&program_objectFix[1].pid);
				gl_texture_bind(&fb_stageB.tex, program_objectFix[1].src, 0);
				gl_texture_bind(&texture_roadmap1, program_objectFix[1].roadmapT1, 1);
				gl_frameBuffer_bind(&fb_stageA.fbo, gl_frameBuffer_clearColor);
				gl_mesh_draw(&mesh_persp, 0, 0);
			}

			/* Refine edge, thinning the thick edge */ {
				gl_program_use(&program_edgeRefine.pid);
				gl_texture_bind(&fb_stageA.tex, program_edgeRefine.src, 0);
				gl_texture_bind(&texture_roadmap1, program_edgeRefine.roadmapT1, 1);
				gl_frameBuffer_bind(&fb_stageB.fbo, gl_frameBuffer_clearColor);
				gl_mesh_draw(&mesh_persp, 0, 0);
			}

			/* Project from perspective to orthographic */ {
				gl_program_use(&program_projectP2O.pid);
				gl_texture_bind(&fb_stageB.tex, program_projectP2O.src, 0);
				gl_texture_bind(&texture_roadmap2, program_projectP2O.roadmapT2, 1);
				gl_frameBuffer_bind(&fb_object[current_obj].fbo, gl_frameBuffer_clearColor);
				gl_mesh_draw(&mesh_ortho, 0, 0);
			}

			/* Measure the distance of edge moving between current frame and previous frame */ {
				gl_program_use(&program_measure.pid);
				gl_texture_bind(&fb_object[current_obj].tex, program_measure.current, 0);
				gl_texture_bind(&fb_object[hint_obj].tex, program_measure.hint, 1);
				gl_texture_bind(&fb_object[previous_obj].tex, program_measure.previous, 2);
				gl_texture_bind(&texture_roadmap1, program_measure.roadmapT1, 3);
				gl_texture_bind(&texture_roadmap2, program_measure.roadmapT2, 4);
				gl_frameBuffer_bind(&fb_stageA.fbo, gl_frameBuffer_clearColor);
				gl_mesh_draw(&mesh_ortho, 0, 0);
			}

			/* Project from orthographic to perspective */ {
				gl_program_use(&program_projectO2P.pid);
				gl_texture_bind(&fb_stageA.tex, program_projectO2P.src, 0);
				gl_texture_bind(&texture_roadmap2, program_projectO2P.roadmapT2, 1);
				gl_frameBuffer_bind(&fb_stageB.fbo, gl_frameBuffer_clearColor);
				gl_mesh_draw(&mesh_persp, 0, 0);
			}

			/* Sample measure result, get single point */ {
				gl_program_use(&program_sample.pid);
				gl_texture_bind(&fb_stageB.tex, program_sample.src, 0);
				gl_frameBuffer_bind(&fb_speed.fbo, gl_frameBuffer_clearColor);
				gl_mesh_draw(&mesh_persp, 0, 0);
			}

			/* Process speed data of previous frame on CPU side and draw speedometer display */ {

				/* Note:
				* Tile-based rendering speedometer base on speedmap is expensive, for every pixel in the video, requires process speedometer size neighbor pixels (O() = videoSize * speedometerSize). 
				* Therefore, we need to download the speedmap, construct a VAO then upload it to GPU. This takes videoSize iterations (O() = videoSize). But the cost is an extra frame of delay. 
				* 
				* At this moment, all the render cmds are queued, but may not fully processed. Instead of waiting the GPU finishing processing current frame, 
				* we can have the CPU processing last frame while GPU processing current frame.
				* While GPU processing the video, CPU finished the speedmap analysis. CPU then upload the processed data using VAO. Notces that, this VAO is associated to previous frame. 
				* Now, we are sure the CPU has the VAO ready, but not sure if the GPU is still precessing. Therefore, we need to upload VAO before download current speedmap. 
				* Upload VAO just register the DMA, non-blocking; but download FBO needs the FBO ready on GPU side, which is blocking. 
				* 
				* The size of speedometer mesh, the result of CPU processing, is not large. We do not need to add another stage of pipeline for the uploaidng. 
				*/

				if (frameCnt) { //Do not process frame 0: no history data
					#ifdef USE_PBO_DOWNLOAD
						gl_synchWait(pboDownloadSynch, GL_SYNCH_TIMEOUT);
						gl_synchDelete(pboDownloadSynch);
						speedData = gl_pixelBuffer_downloadFinish(sizeData[0] * sizeData[1] * 2 * sizeof(float));
					#endif
					analysisObj* objPtr = speedAnalysisObj;
					float* instancePtr = instance_speedometer_data;
					int limit = SHADER_SPEEDOMETER_CNT;
					unsigned int edgeTop = road_focusRegionBox.top * sizeData[1], edgeBottom = road_focusRegionBox.bottom * sizeData[1];
					unsigned int edgeLeft = road_focusRegionBox.left * sizeData[0], edgeRight = road_focusRegionBox.right * sizeData[0];
					for (unsigned int y = edgeTop; y <= edgeBottom && limit; y++) { //Get number speed data from downloaded framebuffer
						for (unsigned int x = edgeLeft; x <= edgeRight && limit; x++) {
							float speed = speedData[y][x][0];
							if (speed >= 1.0 && speed <= 255.0) {
								vec2 coordNorm = { (float)x / sizeData[0] , (float)y / sizeData[1] };
								ivec2 coordScreen = {x,y};
								ivec2 coordRoadmap = { x * road_geoMap.width / sizeData[0] , y * road_geoMap.height / sizeData[1] };
								roadmap_t1 geoData = road_geoMap.map[ coordRoadmap.y * road_geoMap.width + coordRoadmap.x ];
								*objPtr++ = (analysisObj){
									.rx = geoData.px,
									.ry = geoData.py,
									.sx = coordScreen.x,
									.sy = coordScreen.y,
									.speed = speed,
									.osy = speedData[y][x][1]
								};
								*instancePtr++ = coordNorm.x;
								*instancePtr++ = coordNorm.y;
								*instancePtr++ = speed;
								limit--;
							}
						}
					}
					#ifdef USE_PBO_DOWNLOAD
						gl_pixelBuffer_downloadDiscard();
					#endif

					int objCnt = SHADER_SPEEDOMETER_CNT - limit;
					gl_instance_update(&instance_speedometer, 0, objCnt * 3 * sizeof(float), instance_speedometer_data);
					gl_rsync(); //Request process immediately
					gl_frameBuffer_bind(&fb_display.fbo, gl_frameBuffer_clearColor);
					if (objCnt) {
						gl_program_use(&program_display.pid);
						gl_texture_bind(&texture_speedometer, program_display.glyphmap, 0);
						gl_mesh_draw(&mesh_display, 0, objCnt);
						fprintf(stdout, "F %u : %u\n", frameCnt-1, objCnt);
						for (analysisObj* ptr = speedAnalysisObj; ptr < speedAnalysisObj + objCnt; ptr++)
							fprintf(stdout, "O S %.2f : R %.2f,%.2f : S %u,%u : dy %d\n", ptr->speed, ptr->rx, ptr->ry, ptr->sx, ptr->sy, ptr->sy - ptr->osy);
					}
				}

				#ifdef USE_PBO_DOWNLOAD
					gl_pixelBuffer_downloadStart(&pboDownload, &fb_speed.fbo, fb_speed.format, 0, zeros, sizeData);
					pboDownloadSynch = gl_synchSet();
				#else
					gl_frameBuffer_download(speedData, &fb_speed.fbo, fb_speed.format, 0, zeros, sizeData); //Download current speed data so we can process in next iteration (blocking op)
				#endif

			}

//			#define RESULT fb_stageA
//			#define RESULT fb_stageB
//			#define RESULT fb_raw[current]
//			#define RESULT fb_object[current_obj]
//			#define RESULT fb_speed
			#define RESULT fb_display
#endif /* #ifdef ROADMAP_CHECK */

			/* Draw final result on screen */ {
				gl_setViewport(zeros, winsizeNcursor.framesize);
				gl_program_use(&program_final.pid);
				gl_texture_bind(&fb_raw[previous].tex, program_final.orginal, 0);
				gl_texture_bind(&RESULT.tex, program_final.result, 1);
				gl_frameBuffer_bind(NULL, 0);
				gl_mesh_draw(&mesh_final, 0, 0);
			}

			#ifdef DEBUG_THREADSPEED
				debug_threadSpeed = 'M'; //Not critical, no need to use mutex
			#endif

			#ifdef VERBOSE_TIME
				uint64_t timestampRenderEnd = nanotime();
				#ifdef DEBUG_THREADSPEED
					snprintf(winTitle, sizeof(winTitle), "Viewer - frame %u - %c - %.3lf/%.3lf", frameCnt, debug_threadSpeed, (timestampRenderEnd - timestampRenderStart) / (double)1e6, (timestampRenderEnd - timestamp) / (double)1e6);
				#else
					snprintf(winTitle, sizeof(winTitle), "Viewer - frame %u - %.3lf/%.3lf", frameCnt, (timestampRenderEnd - timestampRenderStart) / (double)1e6, (timestampRenderEnd - timestamp) / (double)1e6);
				#endif
				timestamp = timestampRenderEnd;
			#else
				snprintf(winTitle, sizeof(winTitle), "Viewer - frame %u", frameCnt);
			#endif
			
			th_reader_wait(); //Wait reader thread finish uploading frame data
			#ifdef USE_PBO_UPLOAD
				gl_pixelBuffer_updateFinish();
			#endif

			#if FRAME_DELAY
				if (frameCnt > FRAME_DEBUGSKIPSECOND * fps) usleep(FRAME_DELAY);
			#endif

			frameCnt++;
		} else {
		//	float color[4];
		//	gl_frameBuffer_download(color, &RESULT.fbo, RESULT.format, 0, cursorPosData, (const uint[2]){1,1});
		//	snprintf(winTitle, sizeof(winTitle), "Viewer - frame %u, Cursor=(%d,%d), result=(%.4f|%.4f|%.4f|%.4f)", frameCnt, cursorPosData[0], cursorPosData[1], color[0], color[1], color[2], color[3]);
			usleep(50000);
		}

		gl_drawEnd(winTitle);
	}

	/* Free all resources, house keeping */
	status = EXIT_SUCCESS;
label_exit:

	gl_program_delete(&program_final.pid);
	gl_program_delete(&program_display.pid);
	gl_program_delete(&program_sample.pid);
	gl_program_delete(&program_measure.pid);
	gl_program_delete(&program_edgeRefine.pid);
	gl_program_delete(&program_objectFix[1].pid);
	gl_program_delete(&program_objectFix[0].pid);
	gl_program_delete(&program_changingSensor.pid);
	gl_program_delete(&program_edgeFilter.pid);
	gl_program_delete(&program_blurFilter.pid);
	gl_program_delete(&program_projectO2P.pid);
	gl_program_delete(&program_projectP2O.pid);
	gl_program_delete(&program_roadmapCheck.pid);

	gl_texture_delete(&fb_stageB.tex);
	gl_frameBuffer_delete(&fb_stageB.fbo);
	gl_texture_delete(&fb_stageA.tex);
	gl_frameBuffer_delete(&fb_stageA.fbo);
	gl_texture_delete(&fb_display.tex);
	gl_frameBuffer_delete(&fb_display.fbo);
	gl_texture_delete(&fb_speed.tex);
	gl_frameBuffer_delete(&fb_speed.fbo);
	for (unsigned int i = arrayLength(fb_object); i; i--) {
		gl_texture_delete(&fb_object[i-1].tex);
		gl_frameBuffer_delete(&fb_object[i-1].fbo);
	}
	for (unsigned int i = arrayLength(fb_raw); i; i--) {
		gl_texture_delete(&fb_raw[i-1].tex);
		gl_frameBuffer_delete(&fb_raw[i-1].fbo);
	}
	
	gl_mesh_delete(&mesh_final);

	free(speedAnalysisObj);
	free(speedData);
	#ifdef USE_PBO_DOWNLOAD
		gl_pixelBuffer_delete(&pboDownload);
		gl_synchDelete(pboDownloadSynch);
	#endif

	gl_mesh_delete(&mesh_display);
	gl_instance_delete(&instance_speedometer);
	free(instance_speedometer_data);
	gl_texture_delete(&texture_speedometer);

	gl_texture_delete(&texture_roadmap2);
	gl_texture_delete(&texture_roadmap1);
	gl_mesh_delete(&mesh_ortho);
	gl_mesh_delete(&mesh_persp);

	free(road_geoMap.map);

	th_reader_destroy();
	gl_texture_delete(&texture_orginalBuffer[1]);
	gl_texture_delete(&texture_orginalBuffer[0]);
	#ifdef USE_PBO_UPLOAD
		gl_pixelBuffer_delete(&pboUpload[1]);
		gl_pixelBuffer_delete(&pboUpload[0]);
	#else
		free(rawData[0]);
		free(rawData[1]);
	#endif

	gl_destroy();

	info("\n%u frames displayed.\n", frameCnt);
	return status;
}