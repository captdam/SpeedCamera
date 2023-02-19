#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>

#include <GL/glew.h>
#include <GL/glfw3.h>

#include "common.h"
#include "gl.h"
#include "roadmap.h"
#include "speedometer.h"
#include "th_reader.h"
#include "th_output.h"

/* Program config */
#define MAX_SPEED 200 //km/h
#define GL_SYNCH_TIMEOUT 5000000000LLU //For gl sync timeout

/* Shader config */
//#define HEADLESS
#define SHADER_DIR "fshader/"
#define SHADER_MEASURE_INTERLACE 7 //Must be 2^n - 1 (1, 3, 7, 15...), this create a 2^n level queue
#define SHADER_SPEED_DOWNLOADLATENCY 7 //Must be 2^n - 1 (1, 3, 7, 15...), this create a 2^n level queue. Higher number means higher chance the FBO is ready when download, lower stall but higher latency as well
#define SHADER_SPEEDOMETER_CNT 32 //Max number of speedometer

/* Speedometer */
#define SPEEDOMETER_FILE "./textmap.data"

/* Video data upload to GPU and processed data download to CPU */
//#define USE_PBO_UPLOAD //Not big gain: uploading is asynch op, driver will copy data to internal buffer and then upload
#define USE_PBO_DOWNLOAD //Big gain: download is synch op

#define TEXUNIT_ROADMAP 15 //Reserve binding point for reference texture data to reduce texture re-binding
#define TEXUNIT_SPEEDOLMETER 14

#define info(format, ...) {fprintf(stderr, "Log:\t"format"\n" __VA_OPT__(,) __VA_ARGS__);} //Write log
#define error(format, ...) {fprintf(stderr, "Err:\t"format"\n" __VA_OPT__(,) __VA_ARGS__);} //Write error log

int main(int argc, char* argv[]) {
	const uint zeros[4] = {0, 0, 0, 0}; //Zero array with 4 elements (can be used as zeros[1], zeros[2] or zeros[3] as well, it is just a pointer in C)

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

	//Roadinfo, a mesh to store region of interest, and texture to store road-domain data
	roadmap roadmap = ROADMAP_DEFAULTSTRUCT;
	gl_mesh mesh_persp = GL_INIT_DEFAULT_MESH;
	gl_mesh mesh_ortho = GL_INIT_DEFAULT_MESH;
	gl_tex texture_roadmap = GL_INIT_DEFAULT_TEX; //2D array texture: 1 - Geo coord in persp and ortho views; 2 - Up and down search limit, P2O and O2P project lookup
	struct {
		float left, right, top, bottom;
	} road_boxROI;

	//To display human readable text on screen
	gl_tex texture_speedometer = GL_INIT_DEFAULT_TEX; //Glyph
	float* instance_speedometer_data = NULL; //Speed data to be draw (sx, sy, speed)
	gl_mesh mesh_display = GL_INIT_DEFAULT_MESH;

	//Analysis and export data (CPU side)
	uint8_t (* speedData)[sizeData[0]][2] = NULL; //FBO download, height(sizeData[1]) * width(sizeData[0]) * RG8. Note: no performance difference between RGBA8 and RG8 on VC6
	#ifdef USE_PBO_DOWNLOAD
		gl_synch pboDownloadSynch = NULL;
		gl_pbo pboDownload = GL_INIT_DEFAULT_PBO;
	#endif

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
		[0 ... 1] = {GL_INIT_DEFAULT_FBO, GL_INIT_DEFAULT_TEX, gl_texformat_RGBA8} //Input video, RGBA8
	};
	fb fb_object[SHADER_MEASURE_INTERLACE + 1] = { //Object detection of current and previous frames
		[0 ... SHADER_MEASURE_INTERLACE] = {GL_INIT_DEFAULT_FBO, GL_INIT_DEFAULT_TEX, gl_texformat_R8} //Enum < 256
	};
	fb fb_speed[SHADER_SPEED_DOWNLOADLATENCY + 1] = { //Speed measure result, road-domain speed in km/h and screen-domain speed in px/frame
		[0 ... SHADER_SPEED_DOWNLOADLATENCY] = {GL_INIT_DEFAULT_FBO, GL_INIT_DEFAULT_TEX, gl_texformat_RG8} //Range uint8 [0, 255]
	};
	fb fb_display = {GL_INIT_DEFAULT_FBO, GL_INIT_DEFAULT_TEX, gl_texformat_RGBA8}; //Display human-readable text, video, RGBA8
	fb fb_stageA = {GL_INIT_DEFAULT_FBO, GL_INIT_DEFAULT_TEX, gl_texformat_RG8}; //General intermediate data, normalized, max 2 ch
	fb fb_stageB = {GL_INIT_DEFAULT_FBO, GL_INIT_DEFAULT_TEX, gl_texformat_RG8};
	//fb fb_check = {GL_INIT_DEFAULT_FBO, GL_INIT_DEFAULT_TEX, gl_texformat_RGBA16F};

	//Program - Roadmap check
	struct { gl_program pid; } program_roadmapCheck = {.pid = GL_INIT_DEFAULT_PROGRAM};
	struct { gl_program pid; gl_param src; } program_projectP2O = {.pid = GL_INIT_DEFAULT_PROGRAM};
	struct { gl_program pid; gl_param src; } program_projectO2P = {.pid = GL_INIT_DEFAULT_PROGRAM};
	struct { gl_program pid; gl_param src; } program_blurFilter = {.pid = GL_INIT_DEFAULT_PROGRAM};
	struct { gl_program pid; gl_param src; } program_edgeFilter = {.pid = GL_INIT_DEFAULT_PROGRAM};
	struct { gl_program pid; gl_param current; gl_param previous; } program_changingSensor = {.pid = GL_INIT_DEFAULT_PROGRAM};
	struct { gl_program pid; gl_param src; gl_param direction; } program_objectFix = {.pid = GL_INIT_DEFAULT_PROGRAM};
	struct { gl_program pid; gl_param src; } program_edgeRefine = {.pid = GL_INIT_DEFAULT_PROGRAM};
	struct { gl_program pid; gl_param current; gl_param hint; gl_param previous; } program_measure = {.pid = GL_INIT_DEFAULT_PROGRAM};
	struct { gl_program pid; gl_param src; } program_sample = {.pid = GL_INIT_DEFAULT_PROGRAM};
	struct { gl_program pid; } program_display = {.pid = GL_INIT_DEFAULT_PROGRAM};
	struct { gl_program pid; gl_param orginal; gl_param result; } program_final = {.pid = GL_INIT_DEFAULT_PROGRAM};

	/* Init OpenGL and viewer window */ {
		info("Init openGL...");
		if (!gl_init((gl_config){
			.vMajor = 3,
			.vMinor = 1,
			.gles = 1,
			.vsynch = 0,
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

		gl_tex_dim dim[3] = {
			{.size = sizeData[0], .wrapping = gl_tex_dimWrapping_edge},
			{.size = sizeData[1], .wrapping = gl_tex_dimWrapping_edge},
			{.size = 0, .wrapping = gl_tex_dimWrapping_edge}
		};
		texture_orginalBuffer[0] = gl_texture_create(gl_texformat_RGBA8, gl_textype_2d, gl_tex_dimFilter_nearest, gl_tex_dimFilter_nearest, dim); //RGBA8 Video use lowp
		texture_orginalBuffer[1] = gl_texture_create(gl_texformat_RGBA8, gl_textype_2d, gl_tex_dimFilter_nearest, gl_tex_dimFilter_nearest, dim);
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

	/* Roadmap: mesh for region of interest, road info, textures for road data */ {
		info("Load resource - roadmap \"%s\"...", roadmapFile);
		char* statue;
		roadmap = roadmap_init(roadmapFile, &statue);
		if (statue) {
			error("Cannot load roadmap: %s", statue);
			goto label_exit;
		}
		
		road_boxROI.left = roadmap.roadPoints[roadmap.header.pCnt-4].sx;
		road_boxROI.right = roadmap.roadPoints[roadmap.header.pCnt-1].sx;
		road_boxROI.top = roadmap.roadPoints[roadmap.header.pCnt-4].sy;
		road_boxROI.bottom = roadmap.roadPoints[roadmap.header.pCnt-1].sy;

		roadmap_post(&roadmap, MAX_SPEED / 3.6 / fps, SHADER_MEASURE_INTERLACE * MAX_SPEED / 3.6 / fps); //km/h to m/s to m/frame

		const unsigned int sizeRoadmap[3] = {roadmap.header.width, roadmap.header.height, 1};
		const gl_index_t attributes[] = {2, 0};
		mesh_persp = gl_mesh_create(roadmap.header.pCnt-4, 0, 0, gl_meshmode_triangleStrip, attributes, NULL, (gl_vertex_t*)(roadmap.roadPoints), NULL, NULL);
		mesh_ortho = gl_mesh_create(4, 0, 0, gl_meshmode_triangleStrip, attributes, NULL, (gl_vertex_t*)(roadmap.roadPoints + roadmap.header.pCnt-4), NULL, NULL);
		if ( !gl_mesh_check(&mesh_persp) || !gl_mesh_check(&mesh_ortho) ) {
			roadmap_destroy(&roadmap);
			error("Fail to create mesh for roadmap - Region of interest");
			goto label_exit;
		}
		
		gl_tex_dim dim[3] = {
			{.size = sizeRoadmap[0], .wrapping = gl_tex_dimWrapping_edge},
			{.size = sizeRoadmap[1], .wrapping = gl_tex_dimWrapping_edge},
			{.size = 2, .wrapping = gl_tex_dimWrapping_edge}
		};
		texture_roadmap = gl_texture_create(gl_texformat_RGBA16F, gl_textype_2dArray, gl_tex_dimFilter_nearest, gl_tex_dimFilter_nearest, dim); //Mediump is good enough for road-domain geo locations, f16 (1/1024) is OK for pixel indexing
		if (!gl_texture_check(&texture_roadmap)) {
			roadmap_destroy(&roadmap);
			error("Fail to create texture buffer for roadmap");
			goto label_exit;
		}
		gl_texture_update(&texture_roadmap, roadmap.t1, (const int[3]){0, 0, 0}, sizeRoadmap);
		gl_texture_update(&texture_roadmap, roadmap.t2, (const int[3]){0, 0, 1}, sizeRoadmap);
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
		gl_tex_dim dim[3] = {
			{.size = glyphSize[0], .wrapping = gl_tex_dimWrapping_edge},
			{.size = glyphSize[1], .wrapping = gl_tex_dimWrapping_edge},
			{.size = 256, .wrapping = gl_tex_dimWrapping_edge}
		};
		texture_speedometer = gl_texture_create(gl_texformat_RGBA8, gl_textype_2dArray, gl_tex_dimFilter_nearest, gl_tex_dimFilter_nearest, dim); //RGBA8 image use lowp
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
		const gl_vertex_t vertices[] = {
			+0.02, -0.0125, 1.0f, 0.0f,
			+0.02, +0.0125, 1.0f, 1.0f,
			-0.02, +0.0125, 0.0f, 1.0f,
			-0.02, -0.0125, 0.0f, 0.0f
		};
		mesh_display = gl_mesh_create(4, 0, SHADER_SPEEDOMETER_CNT, gl_meshmode_triangleFan, (gl_index_t[]){4, 0}, (gl_index_t[]){3, 0}, vertices, NULL, NULL);
		if (!gl_mesh_check(&mesh_display)) {
			error("Fail to create mesh to store speedometer");
			goto label_exit;
		}
	}

	/* Create buffer for post process on CPU side & Start output thread for result write */ {
		#ifdef USE_PBO_DOWNLOAD
			pboDownload = gl_pixelBuffer_create(sizeData[0] * sizeData[1] * sizeof(speedData[0][0]), 1, gl_usage_stream);
			if (!gl_pixelBuffer_check(&pboDownload)) {
				error("Fail to create pixel buffer for speed downloading");
				goto label_exit;
			}
		#else
			speedData = malloc(sizeData[0] * sizeData[1] * sizeof(speedData[0][0])); //FBO dump
			if (!speedData) {
				error("Fail to create buffer to download speed framebuffer");
				goto label_exit;
			}
		#endif
		
		info("Init output thread...");
		if (!th_output_init()) {
			error("Fail to create output thread");
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
		mesh_final = gl_mesh_create(4, 0, 0, gl_meshmode_triangleFan, (const gl_index_t[]){2, 0}, NULL, vertices, NULL, NULL);
		if (!gl_mesh_check(&mesh_final)) {
			error("Fail to create mesh for final display");
			goto label_exit;
		}
	}

	/* Create work buffer */ {
		info("Create GPU buffers...");

		gl_tex_dim dim[3] = {
			{.size = sizeData[0], .wrapping = gl_tex_dimWrapping_edge},
			{.size = sizeData[1], .wrapping = gl_tex_dimWrapping_edge},
			{.size = 0, .wrapping = gl_tex_dimWrapping_edge}
		};

		for (unsigned int i = 0; i < arrayLength(fb_raw); i++) {
			fb_raw[i].tex = gl_texture_create(fb_raw[i].format, gl_textype_2d, gl_tex_dimFilter_nearest, gl_tex_dimFilter_nearest, dim); //Video data
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
			fb_object[i].tex = gl_texture_create(fb_object[i].format, gl_textype_2d, gl_tex_dimFilter_nearest, gl_tex_dimFilter_nearest, dim); //Object in video
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

		for (uint i = 0; i < arrayLength(fb_speed); i++) {
			fb_speed[i].tex = gl_texture_create(fb_speed[i].format, gl_textype_2d, gl_tex_dimFilter_nearest, gl_tex_dimFilter_nearest, dim); //Speed data, discrete
			if (!gl_texture_check(&fb_speed[i].tex)) {
				error("Fail to create texture to store speed (%u)", i);
				goto label_exit;
			}
			fb_speed[i].fbo = gl_frameBuffer_create(1, (const gl_tex[]){fb_speed[i].tex}, (const gl_fboattach[]){gl_fboattach_color0});
			if (!gl_frameBuffer_check(&fb_speed[i].fbo) ) {
				error("Fail to create FBO to store speed (%u)", i);
				goto label_exit;
			}
		}

		fb_display.tex = gl_texture_create(fb_display.format, gl_textype_2d, gl_tex_dimFilter_nearest, gl_tex_dimFilter_nearest, dim); //Video display to user
		if (!gl_texture_check(&fb_display.tex)) {
			error("Fail to create texture to store speed display");
			goto label_exit;
		}
		fb_display.fbo = gl_frameBuffer_create(1, (const gl_tex[]){fb_display.tex}, (const gl_fboattach[]){gl_fboattach_color0});
		if (!gl_frameBuffer_check(&fb_display.fbo) ) {
			error("Fail to create FBO to store speed display");
			goto label_exit;
		}

		fb_stageA.tex = gl_texture_create(fb_stageA.format, gl_textype_2d, gl_tex_dimFilter_nearest, gl_tex_dimFilter_nearest, dim); //General Data
		fb_stageB.tex = gl_texture_create(fb_stageB.format, gl_textype_2d, gl_tex_dimFilter_nearest, gl_tex_dimFilter_nearest, dim);
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

		//fb_check.tex = gl_texture_create(fb_check.format, gl_textype_2d, gl_tex_dimFilter_nearest, gl_tex_dimFilter_nearest, dim);
		//fb_check.fbo = gl_frameBuffer_create(1, (const gl_tex[]){fb_check.tex}, (const gl_fboattach[]){gl_fboattach_color0});
	}

	/* Load shader programs */ {
		info("Load shaders...");
		gl_program_setCommonHeader("#version 310 es\n");
		#define NL "\n"

		/* Create program: Roadmap check */ {
			gl_programArg arg[] = {
				{gl_programArgType_normal,	"roadmap"},
				{.name = NULL}
			};

			if (!( program_roadmapCheck.pid = gl_program_load(SHADER_DIR"roadmapCheck.glsl", arg) )) {
				error("Fail to create shader program: Roadmap check");
				goto label_exit;
			}

			gl_program_use(&program_roadmapCheck.pid);
			gl_texture_bind(&texture_roadmap, arg[0].id, TEXUNIT_ROADMAP);
		}

		/* Create program: Project P2O and P2O */ {
			gl_programArg arg[] = {
				{gl_programArgType_normal,	"src"},
				{gl_programArgType_normal,	"roadmap"},
				{.name = NULL}
			};

			if (!( program_projectP2O.pid = gl_program_load(SHADER_DIR"projectP2O.glsl", arg) )) {
				error("Fail to create shader program: Project perspective to orthographic");
				goto label_exit;
			}
			program_projectP2O.src = arg[0].id;

			gl_program_use(&program_projectP2O.pid);
			gl_texture_bind(&texture_roadmap, arg[1].id, TEXUNIT_ROADMAP);

			if (!( program_projectO2P.pid = gl_program_load(SHADER_DIR"projectO2P.glsl", arg) )) {
				error("Fail to create shader program: Project orthographic to perspective");
				goto label_exit;
			}
			program_projectO2P.src = arg[0].id;

			gl_program_use(&program_projectO2P.pid);
			gl_texture_bind(&texture_roadmap, arg[1].id, TEXUNIT_ROADMAP);
		}

		/* Create program: Blur and edge filter*/ {
			gl_programArg arg[] = {
				{gl_programArgType_normal,	"src"},
				{.name = NULL}
			};

			if (!( program_blurFilter.pid = gl_program_load(SHADER_DIR"blurFilter.glsl", arg) )) {
				error("Fail to create shader program: Blur filter");
				goto label_exit;
			}
			program_blurFilter.src = arg[0].id;

			if (!( program_edgeFilter.pid = gl_program_load(SHADER_DIR"edgeFilter.glsl", arg) )) {
				error("Fail to create shader program: Edge filter");
				goto label_exit;
			}
			program_edgeFilter.src = arg[0].id;
		}
		
		/* Create program: Changing sensor */ {
			gl_programArg arg[] = {
				{gl_programArgType_normal,	"current"},
				{gl_programArgType_normal,	"previous"},
				{.name = NULL}
			};

			if (!( program_changingSensor.pid = gl_program_load(SHADER_DIR"changingSensor.glsl", arg) )) {
				error("Fail to create shader program: Changing sensor");
				goto label_exit;
			}
			program_changingSensor.current = arg[0].id;
			program_changingSensor.previous = arg[1].id;
		}

		/* Create program: Object fix */ {
			gl_programArg arg[] = {
				{gl_programArgType_normal,	"src"},
				{gl_programArgType_normal,	"roadmap"},
				{gl_programArgType_normal,	"direction"},
				{.name = NULL}
			};

			if (!( program_objectFix.pid = gl_program_load(SHADER_DIR"objectFix.glsl", arg) )) {
				error("Fail to create shader program: Object fix");
				goto label_exit;
			}
			program_objectFix.src = arg[0].id;
			program_objectFix.direction = arg[2].id;

			gl_program_use(&program_objectFix.pid);
			gl_texture_bind(&texture_roadmap, arg[1].id, TEXUNIT_ROADMAP);
		}

		/* Create program: Edge refine */ {
			gl_programArg arg[] = {
				{gl_programArgType_normal,	"src"},
				{gl_programArgType_normal,	"roadmap"},
				{.name = NULL}
			};

			if (!( program_edgeRefine.pid = gl_program_load(SHADER_DIR"edgeRefine.glsl", arg) )) {
				error("Fail to create shader program: Edge refine");
				goto label_exit;
			}
			program_edgeRefine.src = arg[0].id;

			gl_program_use(&program_edgeRefine.pid);
			gl_texture_bind(&texture_roadmap, arg[1].id, TEXUNIT_ROADMAP);
		}

		/* Create program: Measure */ {
			gl_programArg arg[] = {
				{gl_programArgType_normal,	"current"},
				{gl_programArgType_normal,	"hint"},
				{gl_programArgType_normal,	"previous"},
				{gl_programArgType_normal,	"roadmap"},
				{gl_programArgType_normal,	"bias"},
				{.name = NULL}
			};

			if (!( program_measure.pid = gl_program_load(SHADER_DIR"measure.glsl", arg) )) {
				error("Fail to create shader program: Measure");
				goto label_exit;
			}
			program_measure.current = arg[0].id;
			program_measure.hint = arg[1].id;
			program_measure.previous = arg[2].id;

			gl_program_use(&program_measure.pid);
			gl_texture_bind(&texture_roadmap, arg[3].id, TEXUNIT_ROADMAP);
			gl_program_setParam(arg[4].id, 1, gl_datatype_float, (const float[1]){fps * 3.6f / SHADER_MEASURE_INTERLACE}); //m/Nframe to m/frame to km/frame
		}

		/* Create program: Sample */ {
			gl_programArg arg[] = {
				{gl_programArgType_normal,	"src"},
				{.name = NULL}
			};

			if (!( program_sample.pid = gl_program_load(SHADER_DIR"sample.glsl", arg) )) {
				error("Fail to create shader program: Sample");
				goto label_exit;
			}
			program_sample.src = arg[0].id;
		}

		/* Create program: Display */ {
			gl_programArg arg[] = {
				{gl_programArgType_normal,	"glyphmap"},
				{.name = NULL}
			};

			if (!( program_display.pid = gl_program_load(SHADER_DIR"display.glsl", arg) )) {
				error("Fail to create shader program: Display");
				goto label_exit;
			}

			gl_program_use(&program_display.pid);
			gl_texture_bind(&texture_speedometer, arg[0].id, TEXUNIT_SPEEDOLMETER);
		}

		/* Create program: Final display */ {
			gl_programArg arg[] = {
				{gl_programArgType_normal,	"orginalTexture"},
				{gl_programArgType_normal,	"processedTexture"},
				{.name = NULL}
			};

			if (!( program_final.pid = gl_program_load(SHADER_DIR"final.glsl", arg) )) {
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
	fprintf(stdout, "R %u*%u : I %u\n", sizeData[0], sizeData[1], SHADER_MEASURE_INTERLACE);
	
	/* Main process loop here */
	uint current, previous; //Two level queue
	uint current_obj, hint_obj, previous_obj; //Object queue
	uint current_speed, previous_speed; //Speedmap queue
	while(!gl_close(-1)) {
		gl_drawStart();
		char winTitle[200];

		gl_winsizeNcursor winsizeNcursor = gl_getWinsizeCursor();
		int cursorPosData[2] = {winsizeNcursor.curPos[0] * sizeData[0], winsizeNcursor.curPos[1] * sizeData[1]};

		if (!inBox(cursorPosData[0], cursorPosData[1], 0, sizeData[0], 0, sizeData[1], -1)) {
			current = (uint)frameCnt & (uint)0b1 ? 1 : 0; //Front
			previous = 1 - current; //Back
			current_obj = (uint)frameCnt & (uint)SHADER_MEASURE_INTERLACE;
			hint_obj = ( (uint)frameCnt - (uint)1 ) & (uint)SHADER_MEASURE_INTERLACE;
			previous_obj = ( (uint)frameCnt + (uint)1 ) & (uint)SHADER_MEASURE_INTERLACE;
			current_speed = (uint)frameCnt & (uint)SHADER_SPEED_DOWNLOADLATENCY;
			previous_speed = ( (uint)frameCnt + (uint)1 ) & (uint)SHADER_SPEED_DOWNLOADLATENCY;
		
			// Asking the read thread to upload next frame while the main thread processing current frame
			void* reader_addr; // Note: 3-stage uploading scheme: reader thread - main thread uploading - GPU processing
			#ifdef USE_PBO_UPLOAD
				gl_pixelBuffer_updateToTexture(&pboUpload[current], &texture_orginalBuffer[previous]);
				reader_addr = gl_pixelBuffer_updateStart(&pboUpload[previous], sizeData[0] * sizeData[1] * 4);
			#else
				gl_texture_update(&texture_orginalBuffer[previous], rawData[current], zeros, sizeData);
				reader_addr = rawData[previous];
			#endif
			th_reader_start(reader_addr);

			// Start Download data processed in previous frame (if use PBO), this call starts download in background, non-stall
			#ifdef USE_PBO_DOWNLOAD
				gl_pixelBuffer_downloadStart(&pboDownload, &fb_speed[previous_speed].fbo, fb_speed->format, 0, zeros, sizeData);
				//pboDownloadSynch = gl_synchSet(); //See no difference, probally because PBO download must be synch
			#endif

			#ifdef VERBOSE_TIME
				uint64_t timestampRenderStart = nanotime();
			#endif
			//gl_rsync(); //Request the GL driver start the queue

			gl_setViewport(zeros, sizeData);

			// Debug use ONLY: Check roadmap
			/*gl_frameBuffer_bind(&fb_check.fbo, gl_frameBuffer_clearAll);
			gl_program_use(&program_roadmapCheck.pid);
			gl_mesh_draw(&mesh_final, 0, 0);*/

			// Blur the raw to remove noise
			gl_frameBuffer_bind(&fb_raw[current].fbo, gl_frameBuffer_clearAll); //Mesa: Clear buffer allows the driver to discard old buffer (the doc says it is faster)
			gl_program_use(&program_blurFilter.pid);
			gl_texture_bind(&texture_orginalBuffer[current], program_blurFilter.src, 0);
			gl_mesh_draw(&mesh_final, 0, 0); //Process the entire scene. Although we only need to process ROI, but we want to display the entir scene

			// Finding changing to detect moving object
			gl_frameBuffer_bind(&fb_stageA.fbo, gl_frameBuffer_clearAll);
			gl_program_use(&program_changingSensor.pid);
			gl_texture_bind(&fb_raw[current].tex, program_changingSensor.current, 0);
			gl_texture_bind(&fb_raw[previous].tex, program_changingSensor.previous, 1);
			gl_mesh_draw(&mesh_persp, 0, 0);

			// Fix object
			gl_frameBuffer_bind(&fb_stageB.fbo, gl_frameBuffer_clearAll);
			gl_program_use(&program_objectFix.pid);
			gl_program_setParam(program_objectFix.direction, 2, gl_datatype_float, (const float[2]){1, 0}); //Has more gap pixels, needs better cache locality (horizontal pixels are togerther in memory)
			gl_texture_bind(&fb_stageA.tex, program_objectFix.src, 0);
			gl_mesh_draw(&mesh_persp, 0, 0);

			gl_frameBuffer_bind(&fb_stageA.fbo, gl_frameBuffer_clearAll);
			gl_program_setParam(program_objectFix.direction, 2, gl_datatype_float, (const float[2]){0, 1}); //Most gap removed by h-fix, less gap and higher chance of intercepted
			gl_texture_bind(&fb_stageB.tex, program_objectFix.src, 0);
			gl_mesh_draw(&mesh_persp, 0, 0);

			// Refine edge, thinning the thick edge
			gl_frameBuffer_bind(&fb_stageB.fbo, gl_frameBuffer_clearAll);
			gl_program_use(&program_edgeRefine.pid);
			gl_texture_bind(&fb_stageA.tex, program_edgeRefine.src, 0);
			gl_mesh_draw(&mesh_persp, 0, 0);

			// Project from perspective to orthographic
			gl_frameBuffer_bind(&fb_object[current_obj].fbo, gl_frameBuffer_clearAll);
			gl_program_use(&program_projectP2O.pid);
			gl_texture_bind(&fb_stageB.tex, program_projectP2O.src, 0);
			gl_mesh_draw(&mesh_ortho, 0, 0);

			// Measure the distance of edge moving between current frame and previous frame
			gl_frameBuffer_bind(&fb_stageA.fbo, gl_frameBuffer_clearAll);
			gl_program_use(&program_measure.pid);
			gl_texture_bind(&fb_object[current_obj].tex, program_measure.current, 0);
			gl_texture_bind(&fb_object[hint_obj].tex, program_measure.hint, 1);
			gl_texture_bind(&fb_object[previous_obj].tex, program_measure.previous, 2);
			gl_mesh_draw(&mesh_ortho, 0, 0);

			// Project from orthographic to perspective
			gl_frameBuffer_bind(&fb_stageB.fbo, gl_frameBuffer_clearAll);
			gl_program_use(&program_projectO2P.pid);
			gl_texture_bind(&fb_stageA.tex, program_projectO2P.src, 0);
			gl_mesh_draw(&mesh_persp, 0, 0);

			// Sample measure result, get single point
			gl_frameBuffer_bind(&fb_speed[current_speed].fbo, gl_frameBuffer_clearAll);
			gl_program_use(&program_sample.pid);
			gl_texture_bind(&fb_stageB.tex, program_sample.src, 0);
			gl_mesh_draw(&mesh_persp, 0, 0);

			// Download data from previous frame
			#ifdef USE_PBO_DOWNLOAD //Background download starts at beginning of frame
				//gl_synchWait(pboDownloadSynch, GL_SYNCH_TIMEOUT);
				//gl_synchDelete(pboDownloadSynch);
				speedData = gl_pixelBuffer_downloadFinish(sizeData[0] * sizeData[1] * sizeof(speedData[0][0]));
			#else //Command queue of previous frame should be finished by now
				gl_frameBuffer_download(speedData, &fb_speed[previous_speed].fbo, fb_speed->format, 0, zeros, sizeData); //Download current speed data so we can process in next iteration (blocking op)
			#endif

			// Analysis the processed data
			output_data speedAnalysisObj[SHADER_SPEEDOMETER_CNT]; //Not too mush data, OK to use stack

			output_data* objPtr = speedAnalysisObj;
			float* instancePtr = instance_speedometer_data;
			int limit = SHADER_SPEEDOMETER_CNT;
			unsigned int edgeTop = road_boxROI.top * sizeData[1], edgeBottom = road_boxROI.bottom * sizeData[1];
			unsigned int edgeLeft = road_boxROI.left * sizeData[0], edgeRight = road_boxROI.right * sizeData[0];
			for (unsigned int y = edgeTop; y <= edgeBottom && limit; y++) { //Get number speed data from downloaded framebuffer
				for (unsigned int x = edgeLeft; x <= edgeRight && limit; x++) {
					int16_t speed = speedData[y][x][0];
					int16_t screenDy = speedData[y][x][1];
					if (speed > 1) {
						vec2 coordNorm = { (float)x / sizeData[0] , (float)y / sizeData[1] };
						ivec2 coordScreen = {x,y};
						ivec2 coordRoadmap = { x * roadmap.header.width / sizeData[0] , y * roadmap.header.height / sizeData[1] };
						struct Roadmap_Table1 geoData = roadmap.t1[ coordRoadmap.y * roadmap.header.width + coordRoadmap.x ];
						*objPtr++ = (output_data){
							.rx = geoData.px,
							.ry = geoData.py,
							.sx = coordScreen.x,
							.sy = coordScreen.y,
							.speed = speed,
							.osy = screenDy - 128
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

			// Render the analysis result and write to output
			int objCnt = SHADER_SPEEDOMETER_CNT - limit;
			#ifndef HEADLESS //Clean display, disabled in headless mode
				gl_frameBuffer_bind(&fb_display.fbo, gl_frameBuffer_clearAll);
			#endif
			if (objCnt) {
				#ifndef HEADLESS //Upload speed data to display, disabled in headless mode
					gl_frameBuffer_bind(&fb_display.fbo, gl_frameBuffer_clearAll);
					gl_mesh_updateInstances(&mesh_display, instance_speedometer_data, 0, objCnt * 3);
					gl_program_use(&program_display.pid);
					gl_mesh_draw(&mesh_display, 0, objCnt);
				#endif
				th_output_write(frameCnt - 1, objCnt, speedAnalysisObj);
			}

//			#define RESULT fb_stageA
//			#define RESULT fb_stageB
//			#define RESULT fb_raw[current]
//			#define RESULT fb_object[current_obj]
			#define RESULT fb_speed[current_speed]
//			#define RESULT fb_display
			//#define RESULT fb_check

			// Draw final result on screen
			#ifndef HEADLESS //Draw result on display, disabled in headless mode
				gl_setViewport(zeros, winsizeNcursor.framesize);
				gl_frameBuffer_bind(NULL, 0);
				gl_program_use(&program_final.pid);
				gl_texture_bind(&fb_raw[previous].tex, program_final.orginal, 0);
				gl_texture_bind(&RESULT.tex, program_final.result, 1);
				gl_mesh_draw(&mesh_final, 0, 0);
			#endif

			#ifdef VERBOSE_TIME
				uint64_t timestampRenderEnd = nanotime();
				snprintf(winTitle, sizeof(winTitle), "Viewer - frame %u - %.3lf/%.3lf", frameCnt, (timestampRenderEnd - timestampRenderStart) / (double)1e6, (timestampRenderEnd - timestamp) / (double)1e6);
				timestamp = timestampRenderEnd;
			#else
				snprintf(winTitle, sizeof(winTitle), "Viewer - frame %u", frameCnt);
			#endif
			
			th_reader_wait(); //Wait reader thread finish uploading frame data
			#ifdef USE_PBO_UPLOAD
				gl_pixelBuffer_updateFinish();
			#endif

			frameCnt++;
		} else {
			uint8_t x[4];
			float xf[4];
			gl_frameBuffer_download(&RESULT.fbo, x, RESULT.format, 0, cursorPosData, (const uint[2]){1,1});
			snprintf(winTitle, sizeof(winTitle), "Viewer - frame %u, Cursor=(%d,%d), result=(%d|%d|%d|%d)",
				frameCnt, cursorPosData[0], cursorPosData[1], x[0], x[1], x[2], x[3]
			);
			/*gl_frameBuffer_download(&RESULT.fbo, xf, RESULT.format, 0, cursorPosData, (const uint[2]){1,1});
			snprintf(winTitle, sizeof(winTitle), "Viewer - frame %u, Cursor=(%d,%d), result=(%.4f|%.4f|%.4f|%.4f)",
				frameCnt, cursorPosData[0], cursorPosData[1], xf[0], xf[1], xf[2], xf[3]
			);*/
			usleep(50000);
		}

		#ifndef HEADLESS //Do not swap buffer if in headless mode
			gl_drawEnd(winTitle);
		#endif
	}

	/* Free all resources, house keeping */
	status = EXIT_SUCCESS;
label_exit:

	gl_program_delete(&program_final.pid);
	gl_program_delete(&program_display.pid);
	gl_program_delete(&program_sample.pid);
	gl_program_delete(&program_measure.pid);
	gl_program_delete(&program_edgeRefine.pid);
	gl_program_delete(&program_objectFix.pid);
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
	for (uint i = arrayLength(fb_speed); i; i--) {
		gl_texture_delete(&fb_speed[i-1].tex);
		gl_frameBuffer_delete(&fb_speed[i-1].fbo);
	}
	for (unsigned int i = arrayLength(fb_object); i; i--) {
		gl_texture_delete(&fb_object[i-1].tex);
		gl_frameBuffer_delete(&fb_object[i-1].fbo);
	}
	for (unsigned int i = arrayLength(fb_raw); i; i--) {
		gl_texture_delete(&fb_raw[i-1].tex);
		gl_frameBuffer_delete(&fb_raw[i-1].fbo);
	}
	
	gl_mesh_delete(&mesh_final);

	th_output_write(0, -1, NULL);
	th_output_destroy();
	#ifdef USE_PBO_DOWNLOAD
		gl_pixelBuffer_delete(&pboDownload);
//		gl_synchDelete(pboDownloadSynch);
	#else
		free(speedData);
	#endif

	gl_mesh_delete(&mesh_display);
	free(instance_speedometer_data);
	gl_texture_delete(&texture_speedometer);

	gl_texture_delete(&texture_roadmap);
	gl_mesh_delete(&mesh_ortho);
	gl_mesh_delete(&mesh_persp);
	roadmap_destroy(&roadmap);

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