#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <time.h>

#ifdef VERBOSE
#include <errno.h>
#endif

#include <GL/glew.h>
#include <GL/glfw3.h>

#include "common.h"
#include "gl.h"

struct GL_ClassDataStructure {
	GLFWwindow* window;
	gl_mesh defaultMesh;
	gl_shader defaultShader;
	size2d_t windowSize;
	gl_param defaultShader_paramOrginal, defaultShader_paramProcessed;
} this = {
	.window = NULL,
	.defaultMesh = GL_INIT_DEFAULT_MESH,
	.defaultShader = GL_INIT_DEFAULT_SHADER,
	.windowSize = {0, 0}
};

/* Error polling */
void printAllError(const char* id) {
	fprintf(stderr, "==== Error log, until ID: '%s' =================\n", id);
	GLenum error;
	while (1) {
		error = glGetError();
		if (error == GL_NO_ERROR) break;
		fprintf(stderr, "- GL error: %x\n", error);
	}
	fprintf(stderr, "==== End of error list (%s) ====================\n\n", id);
}

/* Texture data encode */
const struct GL_TexFormat_LookUp {
	gl_texformat eFormat;
	GLint internalFormat;
	GLenum format;
	GLenum type;
} gl_texformat_lookup[gl_texformat_placeholderEnd] = {
	{gl_texformat_R8,		GL_R8,		GL_RED,			GL_UNSIGNED_BYTE	},
	{gl_texformat_RG8,		GL_RG8,		GL_RG,			GL_UNSIGNED_BYTE	},
	{gl_texformat_RGB8,		GL_RGB8,	GL_RGB,			GL_UNSIGNED_BYTE	},
	{gl_texformat_RGBA8,		GL_RGBA8,	GL_RGBA,		GL_UNSIGNED_BYTE	},
	{gl_texformat_R8I,		GL_R8I,		GL_RED_INTEGER,		GL_BYTE			},
	{gl_texformat_RG8I,		GL_RG8I,	GL_RG_INTEGER,		GL_BYTE			},
	{gl_texformat_RGB8I,		GL_RGB8I,	GL_RGB_INTEGER,		GL_BYTE			},
	{gl_texformat_RGBA8I,		GL_RGBA8I,	GL_RGBA_INTEGER,	GL_BYTE			},
	{gl_texformat_R8UI,		GL_R8UI,	GL_RED_INTEGER,		GL_UNSIGNED_BYTE	},
	{gl_texformat_RG8UI,		GL_RG8UI,	GL_RG_INTEGER,		GL_UNSIGNED_BYTE	},
	{gl_texformat_RGB8UI,		GL_RGB8UI,	GL_RGB_INTEGER,		GL_UNSIGNED_BYTE	},
	{gl_texformat_RGBA8UI,		GL_RGBA8UI,	GL_RGBA_INTEGER,	GL_UNSIGNED_BYTE	},
	{gl_texformat_R16F,		GL_R16F,	GL_RED,			GL_FLOAT		},
	{gl_texformat_RG16F,		GL_RG16F,	GL_RG,			GL_FLOAT		},
	{gl_texformat_RGB16F,		GL_RGB16F,	GL_RGB,			GL_FLOAT		},
	{gl_texformat_RGBA16F,		GL_RGBA16F,	GL_RGBA,		GL_FLOAT		},
	{gl_texformat_R16I,		GL_R16I,	GL_RED_INTEGER,		GL_SHORT		},
	{gl_texformat_RG16I,		GL_RG16I,	GL_RG_INTEGER,		GL_SHORT		},
	{gl_texformat_RGB16I,		GL_RGB16I,	GL_RGB_INTEGER,		GL_SHORT		},
	{gl_texformat_RGBA16I,		GL_RGBA16I,	GL_RGBA_INTEGER,	GL_SHORT		},
	{gl_texformat_R16UI,		GL_R16UI,	GL_RED_INTEGER,		GL_UNSIGNED_SHORT	},
	{gl_texformat_RG16UI,		GL_RG16UI,	GL_RG_INTEGER,		GL_UNSIGNED_SHORT	},
	{gl_texformat_RGB16UI,		GL_RGB16UI,	GL_RGB_INTEGER,		GL_UNSIGNED_SHORT	},
	{gl_texformat_RGBA16UI,		GL_RGBA16UI,	GL_RGBA_INTEGER,	GL_UNSIGNED_SHORT	},
	{gl_texformat_R32F,		GL_R32F,	GL_RED,			GL_FLOAT		},
	{gl_texformat_RG32F,		GL_RG32F,	GL_RG,			GL_FLOAT		},
	{gl_texformat_RGB32F,		GL_RGB32F,	GL_RGB,			GL_FLOAT		},
	{gl_texformat_RGBA32F,		GL_RGBA32F,	GL_RGBA,		GL_FLOAT		},
	{gl_texformat_R32I,		GL_R32I,	GL_RED_INTEGER,		GL_INT			},
	{gl_texformat_RG32I,		GL_RG32I,	GL_RG_INTEGER,		GL_INT			},
	{gl_texformat_RGB32I,		GL_RGB32I,	GL_RGB_INTEGER,		GL_INT			},
	{gl_texformat_RGBA32I,		GL_RGBA32I,	GL_RGBA_INTEGER,	GL_INT			},
	{gl_texformat_R32UI,		GL_R32UI,	GL_RED_INTEGER,		GL_UNSIGNED_INT		},
	{gl_texformat_RG32UI,		GL_RG32UI,	GL_RG_INTEGER,		GL_UNSIGNED_INT		},
	{gl_texformat_RGB32UI,		GL_RGB32UI,	GL_RGB_INTEGER,		GL_UNSIGNED_INT		},
	{gl_texformat_RGBA32UI,		GL_RGBA32UI,	GL_RGBA_INTEGER,	GL_UNSIGNED_INT		}
};
//GL_TexFormat_LookUp.eFormat should be in increase order and start from 0
int gl_texformat_lookup_check() {
	for (int i = 0; i < gl_texformat_placeholderEnd; i++) {
		if (gl_texformat_lookup[i].eFormat != i)
			return 0;
	}
	return 1;
}

/* Synch commands */
gl_synch gl_synchSet_internal() { //Actual code
	gl_synch s = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	return s;
}
gl_synch gl_synchSet_placeholder() { //Placeholder for GPU without this function
	return NULL;
}
gl_synch (*gl_synchSet_ptr)();

gl_synch_statue gl_synchWait_internal(gl_synch s, uint64_t timeout) {
	gl_synch_statue statue;
	switch (glClientWaitSync(s, GL_SYNC_FLUSH_COMMANDS_BIT, timeout)) {
		case GL_ALREADY_SIGNALED:
			statue = gl_synch_done;
			break;
		case GL_CONDITION_SATISFIED:
			statue = gl_synch_ok;
			break;
		case GL_TIMEOUT_EXPIRED:
			statue = gl_synch_timeout;
			break;
		default:
			statue = gl_synch_error;
			break;
	}
	return statue;
}
gl_synch_statue gl_synchWait_placeholder(gl_synch s, uint64_t timeout) {
	gl_fsync();
	return gl_synch_ok;
}
gl_synch_statue (*gl_synchWait_ptr)(gl_synch s, uint64_t timeout);

void gl_synchDelete_internal(gl_synch s) {
	glDeleteSync(s);
}
void gl_synchDelete_placeholder(gl_synch s) {
	return;
}
void (*gl_synchDelete_ptr)();

/* == Window management and driver init == [Static] ========================================= */

// Event callback when window closed by user (X button or kill)
void gl_windowCloseCallback(GLFWwindow* window) {
	fputs("Window close event fired.\n", stdout);
	glfwSetWindowShouldClose(window, GLFW_TRUE);
}

// GLFW error log. Non-critical error, program should not be killed in this case, just print them in stderr
void gl_glfwErrorCallback(int code, const char* desc) {
	fprintf(stderr, "GLFW error %d: %s\n", code, desc);
}

// OpenGL error log. Non-critical error, program should not be killed in this case, just print them in stderr
void GLAPIENTRY gl_glErrorCallback(GLenum src, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
	fprintf(stderr, "OpenGL info: %s type %x, severity %x, message: %s\n", (type == GL_DEBUG_TYPE_ERROR ? "ERROR" : ""), type, severity, message);
}

int gl_init(size2d_t frameSize, unsigned int windowRatio, float mix) {
	#ifdef VERBOSE
		fputs("Init OpenGL\n", stdout);
		fflush(stdout);
	#endif

	/* Program check */
	if (!gl_texformat_lookup_check()) {
		#ifdef VERBOSE
			fputs("\tProgram error: gl_texformat_lookup - Program-time error\n", stderr);
		#endif
		return 0;
	}

	/* Object init */
	if (this.windowSize.x != 0 || this.windowSize.y != 0) {
		#ifdef VERBOSE
			fputs("Double init OpenGL\n", stderr);
		#endif
		return 0;
	}

	/* init GLFW */
	if (!glfwInit()) {
		#ifdef VERBOSE
			fputs("\tFail to init GLFW\n", stderr);
		#endif
		gl_destroy(this);
		return 0;
	}

	/* Start and config OpenGL, init window */
	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
//	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
	#ifdef VERBOSE
//		glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
	#endif
	this.windowSize = (size2d_t){.width = frameSize.width / windowRatio, .height = frameSize.height / windowRatio};
	this.window = glfwCreateWindow(this.windowSize.width, this.windowSize.height, "Viewer", NULL, NULL);
	if (!this.window){
		#ifdef VERBOSE
			fputs("\tFail to open window\n", stderr);
		#endif
		gl_destroy();
		return 0;
	}
	glfwMakeContextCurrent(this.window);
	glfwSwapInterval(0);

	/* init GLEW */
	GLenum glewInitError = glewInit();
	if (glewInitError != GLEW_OK) {
		#ifdef VERBOSE
			fprintf(stderr, "\tFail init GLEW: %s\n", glewGetErrorString(glewInitError));
		#endif
		gl_destroy(this);
		return 0;
	}

	/* Setup functions and their alternative placeholder */
	if (glFenceSync) {
		gl_synchSet_ptr = &gl_synchSet_internal;
		gl_synchWait_ptr = &gl_synchWait_internal;
		gl_synchDelete_ptr = &gl_synchDelete_internal;
	} else {
		gl_synchSet_ptr = &gl_synchSet_placeholder;
		gl_synchWait_ptr = &gl_synchWait_placeholder;
		gl_synchDelete_ptr = &gl_synchDelete_placeholder;
	}

	/* Event control - callback */
	glfwSetWindowCloseCallback(this.window, gl_windowCloseCallback);
	glfwSetErrorCallback(gl_glfwErrorCallback);
	glDebugMessageCallback(gl_glErrorCallback, NULL);

	/* OpenGL config */
//	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
//	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
//	glLineWidth(10);
	glEnable(GL_PROGRAM_POINT_SIZE);
	glPointSize(10.0);

	/* Draw window - mesh */ {
		gl_vertex_t vertices[] = {
			1.0f, 0.0f,
			1.0f, 1.0f,
			0.0f, 1.0f,
			0.0f, 0.0f
		};
		gl_index_t indices[] = {0, 3, 2, 0, 2, 1};
		gl_index_t attributes[] = {2};
		this.defaultMesh = gl_mesh_create((size2d_t){.height = 4, .width = 2}, 6, attributes, vertices, indices, gl_meshmode_triangles);
	}

	/* Draw window - shader */ {
		gl_shaderSrc vs[] = {
			{.isFile = 0, .src = "#version 310 es\n"},
			{.isFile = 0, .src = "precision mediump float;\n"},
			{.isFile = 0, .src = \
				"layout (location = 0) in vec2 position;\n"
				"out vec2 textpos;\n"
				"void main() {\n"
				"	gl_Position = vec4(position.x * 2.0 - 1.0, position.y * 2.0 - 1.0, 0.0, 1.0);\n"
				"	textpos = vec2(position.x, 1.0 - position.y); //Fix y-axis difference screen coord\n"
				"}\n"
			}
		};
		gl_shaderSrc fs[] = {
			{.isFile = 0, .src = "#version 310 es\n"},
			{.isFile = 0, .src = "precision mediump float;\n"},
			{.isFile = 0, .src = \
				"in vec2 textpos;\n"
				"uniform sampler2D orginalTexture;\n"
				"uniform sampler2D processedTexture;\n"
				"uniform float mixLevel;\n"
				"out vec4 color;\n"
				"void main() {\n"
				"	vec3 od = texture(orginalTexture, textpos).rgb;\n"
				"	vec3 pd = texture(processedTexture, textpos).rgb;\n"
//				"	vec3 pd = texture(processedTexture, textpos).rrr;\n"
				"	vec3 data = mix(od, pd, mixLevel);\n"
				"	color = vec4(data, 1.0);\n"
				"}\n"
			}
		};
		gl_shaderArg args[] = {
			{.isUBO = 0, .name = "orginalTexture"},
			{.isUBO = 0, .name = "processedTexture"},
			{.isUBO = 0, .name = "mixLevel"}
		};
		this.defaultShader = gl_shader_create((ivec4){arrayLength(vs), arrayLength(fs), 0, arrayLength(args)}, vs, fs, NULL, args);
		this.defaultShader_paramOrginal = args[0].id;
		this.defaultShader_paramProcessed = args[1].id;

		gl_shader_use(&this.defaultShader);
		gl_shader_setParam(args[2].id, 1, gl_type_float, &mix);
	}

	/* Init OK */
	return 1;
}

void* gl_getInfo(gl_info name, void* data) {
	void* r = NULL;
	if (name == gl_info_i1_maxTextureSize) {
		glGetIntegerv(GL_MAX_TEXTURE_SIZE, data);
		r = data;
	} else if (name == gl_info_i1_maxArrayTextureLayers) {
		glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, data);
		r = data;
	} else if (name == gl_info_i1_max3dTextureSize) {
		glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, data);
		r = data;
	} else if (name == gl_info_i1_maxTextureImageUnits) {
		glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, data);
		r = data;
	} else if (name == gl_info_i1_maxVertexTextureImageUnits) {
		glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, data);
		r = data;
	} else if (name == gl_info_s_vendor) {
		r = (char*)glGetString(GL_VENDOR);
	} else if (name == gl_info_s_renderer) {
		r = (char*)glGetString(GL_RENDERER);
	} else if (name == gl_info_s_version) {
		r = (char*)glGetString(GL_VERSION);
	} else if (name == gl_info_s_shadingLanguageVersion) {
		r = (char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
	}
	return r;
}

void gl_drawStart(size2d_t* cursorPos) {
	glfwPollEvents();

	double x, y;
	glfwGetCursorPos(this.window, &x, &y);
	*cursorPos = (size2d_t){.x = x, .y = y}; //Cast from double to size_t
}

void gl_drawWindow(gl_tex* orginalTexture, gl_tex* processedTexture) {
	gl_fb df = {.frame=0};
	gl_frameBuffer_bind(&df, this.windowSize, 0);

	gl_shader_use(&this.defaultShader);
	gl_texture_bind(orginalTexture, this.defaultShader_paramOrginal, 0);
	gl_texture_bind(processedTexture, this.defaultShader_paramProcessed, 1);
	
	gl_mesh_draw(&this.defaultMesh);
}

void gl_drawEnd(const char* title) {
	if (title)
		glfwSetWindowTitle(this.window, title);
	
	glfwSwapBuffers(this.window);
}

int gl_close(int close) {
	if (close > 0)
		glfwSetWindowShouldClose(this.window, GLFW_TRUE);
	else if (close == 0)
		glfwSetWindowShouldClose(this.window, GLFW_FALSE);
	
	return glfwWindowShouldClose(this.window);
}

void gl_destroy() {
	#ifdef VERBOSE
		fputs("Destroy GL class object\n", stdout);
		fflush(stdout);
	#endif

	if (this.window)
		glfwTerminate();

	gl_close(1);
	gl_shader_destroy(&this.defaultShader);
	gl_mesh_delete(&this.defaultMesh);
	this.windowSize = (size2d_t){0, 0};
}

/* == OpenGL routines == [Object] =========================================================== */

/* Load a shader from file to memory, return a pointer to the memory, use free() to free the memory when no longer need */
char* gl_loadFileToMemory(const char* filename, long int* length) {
	*length = 0;
	FILE* fp = fopen(filename, "r");
	if (!fp)
		return NULL; //Error: Cannot open file, *length = 0
	
	fseek(fp, 0, SEEK_END);
	*length = ftell(fp);
	rewind(fp);

	if (*length < 0) {
		return NULL; //Error: Cannot get file length, *length < 0
		fclose(fp);
	}
	
	char* content = malloc(*length);
	if (!content) {
		return NULL; //Error: Cannot allocate memory, *length > 0
		fclose(fp);
	}

	int whatever = fread(content, 1, *length, fp);
	content[*length - 1] = '\0'; //Change the empty new line to null terminator
	fclose(fp);
	return content;
}

gl_shader gl_shader_create(ivec4 count, const gl_shaderSrc* vs, const gl_shaderSrc* fs, const gl_shaderSrc* gs, gl_shaderArg* args) {
	unsigned int countV = count.x, countF = count.y, countG = count.z, countArg = count.w;
	GLint status;

	GLuint shader = 0, shaderV = 0, shaderF = 0, shaderG = 0;
	shader = glCreateProgram(); //Non-zero
	shaderV = glCreateShader(GL_VERTEX_SHADER);
	shaderF = glCreateShader(GL_FRAGMENT_SHADER);
	if (countG)
		shaderG = glCreateShader(GL_GEOMETRY_SHADER);
	if (!shader || !shaderV || !shaderF || (countG && !shaderG)) {
		fputs("\tFail to create shader\n", stderr);
	}
	#ifdef VERBOSE
		fprintf(stdout, "Create shader, program id = %u, vs = %u, fs = %u, gs = %u\n", shader, shaderV, shaderF, shaderG);
	#endif

	/* Vertex shader */ {
		#ifdef VERBOSE
			fprintf(stdout, "\tCompile vertex shader, size = %u\n", countV);
		#endif
		const char* buffer[countV];
		for (unsigned int i = 0; i < countV; i++) {
			if (vs[i].isFile) {
				long int length;
				buffer[i] = gl_loadFileToMemory(vs[i].src, &length);
				#ifdef VERBOSE
					fprintf(stdout, "\t%u - From file: %s (%ld)\n", i, vs[i].src, length);
				#endif
				if (!buffer[i]) {
					if (length == 0)
						fprintf(stderr, "\tFail to open vertex shader file: %s (errno = %d)\n", vs[i].src, errno);
					else if (length < 0)
						fprintf(stderr, "\tFail to get vertex shader length: %s (errno = %d)\n", vs[i].src, errno);
					else
						fputs("\tCannot allocate buffer for vertex shader code\n", stderr);
					for (int j = 0; j < i; j++) {
						if (vs[j].isFile)
							free((void*)buffer[j]);
					}
				}
			} else {
				buffer[i] = vs[i].src;
				#ifdef VERBOSE
					fprintf(stdout, "\t%u - From Memory: @%p\n", i, vs[i].src);
				#endif
			}
		}

		glShaderSource(shaderV, countV, (const GLchar * const*)buffer, NULL);
		glCompileShader(shaderV);

		for (unsigned int i = 0; i < countV; i++) {
			if (vs[i].isFile)
				free((void*)buffer[i]);
		}

		glGetShaderiv(shaderV, GL_COMPILE_STATUS, &status);
		if (!status) {
			GLuint msgLength;
			glGetShaderiv(shaderV, GL_INFO_LOG_LENGTH, &msgLength);
			char compileMsg[msgLength];
			glGetShaderInfoLog(shaderV, msgLength, NULL, compileMsg);
			fprintf(stderr, "\tGL vertex shader error:\n%s\n", compileMsg);
			glDeleteShader(shaderV); //A value of 0 for shader and program will be silently ignored
			glDeleteShader(shaderF);
			glDeleteShader(shaderG);
			glDeleteProgram(shader);
			return GL_INIT_DEFAULT_SHADER;
		}
	}

	/* Fragment shader */ {
		#ifdef VERBOSE
			fprintf(stdout, "\tCompile fragment shader, size = %u\n", countF);
		#endif
		const char* buffer[countF];
		for (unsigned int i = 0; i < countF; i++) {
			if (fs[i].isFile) {
				long int length;
				buffer[i] = gl_loadFileToMemory(fs[i].src, &length);
				#ifdef VERBOSE
					fprintf(stdout, "\t%u - From file: %s (%ld)\n", i, fs[i].src, length);
				#endif
				if (!buffer[i]) {
					if (length == 0)
						fprintf(stderr, "\tFail to open fragment shader file: %s (errno = %d)\n", fs[i].src, errno);
					else if (length < 0)
						fprintf(stderr, "\tFail to get fragment shader length: %s (errno = %d)\n", fs[i].src, errno);
					else
						fputs("\tCannot allocate buffer for fragment shader code\n", stderr);
					for (int j = 0; j < i; j++) {
						if (fs[j].isFile)
							free((void*)buffer[j]);
					}
				}
			} else {
				buffer[i] = fs[i].src;
				#ifdef VERBOSE
					fprintf(stdout, "\t%u - From Memory: @%p\n", i, fs[i].src);
				#endif
			}
		}

		glShaderSource(shaderF, countF, (const GLchar * const*)buffer, NULL);
		glCompileShader(shaderF);

		for (unsigned int i = 0; i < countF; i++) {
			if (fs[i].isFile)
				free((void*)buffer[i]);
		}

		glGetShaderiv(shaderF, GL_COMPILE_STATUS, &status);
		if (!status) {
			GLuint msgLength;
			glGetShaderiv(shaderF, GL_INFO_LOG_LENGTH, &msgLength);
			char compileMsg[msgLength];
			glGetShaderInfoLog(shaderF, msgLength, NULL, compileMsg);
			fprintf(stderr, "\tGL fragment shader error:\n%s\n", compileMsg);
			glDeleteShader(shaderV);
			glDeleteShader(shaderF);
			glDeleteShader(shaderG);
			glDeleteProgram(shader);
			return GL_INIT_DEFAULT_SHADER;
		}
	}

	/* Geometry shader */ if (countG) {
		#ifdef VERBOSE
			fprintf(stdout, "\tCompile geometry shader, size = %u\n", countG);
		#endif
		const char* buffer[countG];
		for (unsigned int i = 0; i < countG; i++) {
			if (gs[i].isFile) {
				long int length;
				buffer[i] = gl_loadFileToMemory(gs[i].src, &length);
				#ifdef VERBOSE
					fprintf(stdout, "\t%u - From file: %s (%ld)\n", i, gs[i].src, length);
				#endif
				if (!buffer[i]) {
					if (length == 0)
						fprintf(stderr, "\tFail to open fragment shader file: %s (errno = %d)\n", gs[i].src, errno);
					else if (length < 0)
						fprintf(stderr, "\tFail to get fragment shader length: %s (errno = %d)\n", gs[i].src, errno);
					else
						fputs("\tCannot allocate buffer for fragment shader code\n", stderr);
					for (int j = 0; j < i; j++) {
						if (gs[j].isFile)
							free((void*)buffer[j]);
					}
				}
			} else {
				buffer[i] = gs[i].src;
				#ifdef VERBOSE
					fprintf(stdout, "\t%u - From Memory: @%p\n", i, gs[i].src);
				#endif
			}
		}

		glShaderSource(shaderG, countG, (const GLchar * const*)buffer, NULL);
		glCompileShader(shaderG);

		for (unsigned int i = 0; i < countG; i++) {
			if (gs[i].isFile)
				free((void*)buffer[i]);
		}

		glGetShaderiv(shaderG, GL_COMPILE_STATUS, &status);
		if (!status) {
			GLuint msgLength;
			glGetShaderiv(shaderG, GL_INFO_LOG_LENGTH, &msgLength);
			char compileMsg[msgLength];
			glGetShaderInfoLog(shaderG, msgLength, NULL, compileMsg);
			fprintf(stderr, "\tGL geometry shader error:\n%s\n", compileMsg);
			glDeleteShader(shaderV);
			glDeleteShader(shaderF);
			glDeleteShader(shaderG);
			glDeleteProgram(shader);
			return GL_INIT_DEFAULT_SHADER;
		}
	}

	/* Link */ {
		#ifdef VERBOSE
			fputs("\tLink program\n", stdout);
		#endif
		glAttachShader(shader, shaderV);
		glAttachShader(shader, shaderF);
		if (countG)
			glAttachShader(shader, shaderG);
		
		glLinkProgram(shader);
		glGetProgramiv(shader, GL_LINK_STATUS, &status);
		if (!status) {
			GLuint msgLength;
			glGetProgramiv(shader, GL_INFO_LOG_LENGTH, &msgLength);
			char compileMsg[msgLength];
			glGetProgramInfoLog(shader, msgLength, NULL, compileMsg);
			fprintf(stderr, "\tGL shader link error:\n%s\n", compileMsg);
			glDeleteShader(shaderV);
			glDeleteShader(shaderF);
			glDeleteShader(shaderG);
			glDeleteProgram(shader);
			return GL_INIT_DEFAULT_SHADER;
		}

		#ifdef VERBOSE
			GLuint binSize;
			glGetProgramiv(shader, GL_PROGRAM_BINARY_LENGTH, &binSize);
			fprintf(stdout, "\t- Size: %u\n", binSize);
		#endif
	}

	/* Bind arguments */ {
		#ifdef VERBOSE
			fputs("\tBind arguments\n", stdout);
		#endif
		for (unsigned int i = 0; i < countArg; i++) {
			if (args[i].isUBO) {
				args[i].id = glGetUniformBlockIndex(shader, args[i].name);
				#ifdef VERBOSE
					fprintf(stdout, "\t%u - Param '%s' (UBO) - ID = %u\n", i, args[i].name, args[i].id);
				#endif
			} else {
				args[i].id = glGetUniformLocation(shader, args[i].name);
				#ifdef VERBOSE
					fprintf(stdout, "\t%u - Param '%s' (uniform) - ID = %u\n", i, args[i].name, args[i].id);
				#endif
			}
		}
	}

	glDeleteShader(shaderV); //Flag set, will be automatically delete when program deleted.
	glDeleteShader(shaderF); //So we don't need to remember the vertex and fragment shaders name.
	glDeleteShader(shaderG);
	return shader;
}

int gl_shader_check(gl_shader* shader) {
	if (*shader == GL_INIT_DEFAULT_SHADER)
		return 0;
	return 1;
}

void gl_shader_use(gl_shader* shader) {
	glUseProgram(*shader);
}

void gl_shader_setParam_internal(gl_param paramId, uint8_t length, gl_datatype type, void* data) {
	if (type == gl_type_int) {
		int* d = data;
		if (length == 1)
			glUniform1i(paramId, *d);
		else if (length == 2)
			glUniform2i(paramId, d[0], d[1]);
		else if (length == 3)
			glUniform3i(paramId, d[0], d[1], d[2]);
		else
			glUniform4i(paramId, d[0], d[1], d[2], d[3]);
	}
	else if (type == gl_type_uint) {
		unsigned int* d = data;
		if (length == 1)
			glUniform1ui(paramId, *d);
		else if (length == 2)
			glUniform2ui(paramId, d[0], d[1]);
		else if (length == 3)
			glUniform3ui(paramId, d[0], d[1], d[2]);
		else
			glUniform4ui(paramId, d[0], d[1], d[2], d[3]);

	}
	else {
		float* d = data;
		if (length == 1)
			glUniform1f(paramId, *d);
		else if (length == 2)
			glUniform2f(paramId, d[0], d[1]);
		else if (length == 3)
			glUniform3f(paramId, d[0], d[1], d[2]);
		else
			glUniform4f(paramId, d[0], d[1], d[2], d[3]);
	}
}

void gl_shader_destroy(gl_shader* shader) {
	if (*shader != GL_INIT_DEFAULT_SHADER)
		glDeleteProgram(*shader);
	*shader = GL_INIT_DEFAULT_SHADER;
}

gl_ubo gl_uniformBuffer_create(unsigned int bindingPoint, size_t size) {
	gl_ubo ubo = GL_INIT_DEFAULT_UBO;

	glGenBuffers(1, &ubo);
	glBindBuffer(GL_UNIFORM_BUFFER, ubo);
	glBufferData(GL_UNIFORM_BUFFER, size, NULL, GL_STATIC_DRAW); //in most case, we set params only at the beginning for only once
	glBindBufferBase(GL_UNIFORM_BUFFER, bindingPoint, ubo);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	return ubo;
}

int gl_uniformBuffer_check(gl_ubo* ubo) {
	if (*ubo == GL_INIT_DEFAULT_UBO)
		return 0;
	return 1;
}

void gl_uniformBuffer_bindShader(unsigned int bindingPoint, gl_shader* shader, gl_param blockId) {
	glUniformBlockBinding(*shader, blockId, bindingPoint);
}

void gl_uniformBuffer_update_internal(gl_ubo* ubo, size_t start, size_t len, void* data) {
	glBindBuffer(GL_UNIFORM_BUFFER, *ubo);
	glBufferSubData(GL_UNIFORM_BUFFER, start, len, data);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void gl_unifromBuffer_delete(gl_ubo* ubo) {
	if (*ubo != GL_INIT_DEFAULT_UBO)
		glDeleteBuffers(1, ubo);
	*ubo = GL_INIT_DEFAULT_UBO;
}

gl_mesh gl_mesh_create(size2d_t vertexSize, size_t indexCount, gl_index_t* elementsSize, gl_vertex_t* vertices, gl_index_t* indices, gl_meshmode mode) {
	int indexDraw = indexCount && indices;

	gl_mesh mesh = GL_INIT_DEFAULT_MESH;
	mesh.drawSize = indexDraw ? indexCount : vertexSize.height;

	glGenVertexArrays(1, &mesh.vao);
	glGenBuffers(1, &mesh.vbo);
	if (indexDraw)
		glGenBuffers(1, &mesh.ebo);
	
	glBindVertexArray(mesh.vao);
	glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
	glBufferData(GL_ARRAY_BUFFER, vertexSize.height * vertexSize.width * sizeof(gl_vertex_t), vertices, GL_STATIC_DRAW);
	if (indexDraw) {
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexCount * sizeof(gl_index_t), indices, GL_STATIC_DRAW);
	}
	
	GLuint elementIndex = 0, attrIndex = 0;
	while (elementIndex < vertexSize.width) {
		glVertexAttribPointer(attrIndex, *elementsSize, GL_FLOAT, GL_FALSE, vertexSize.width * sizeof(float), (GLvoid*)(elementIndex * sizeof(float)));
		glEnableVertexAttribArray(attrIndex);
		elementIndex += *(elementsSize++);
		attrIndex++;
	}

	if (mode == gl_meshmode_triangles)
		mesh.mode = GL_TRIANGLES;
	else if (mode == gl_meshmode_points)
		mesh.mode = GL_POINTS;
	else if (mode == gl_meshmode_triangleFan)
		mesh.mode = GL_TRIANGLE_FAN;
	
	glBindBuffer(GL_ARRAY_BUFFER, GL_INIT_DEFAULT_MESH.vbo);
	glBindVertexArray(GL_INIT_DEFAULT_MESH.vao);
	return mesh;
}

int gl_mesh_check(gl_mesh* mesh) {
	if (mesh->vao == GL_INIT_DEFAULT_MESH.vao)
		return 0;
	if (mesh->vbo == GL_INIT_DEFAULT_MESH.vbo)
		return 0;
	return 1;
}

void gl_mesh_draw(gl_mesh* mesh) {
	if (mesh) {
		glBindVertexArray(mesh->vao);
		if (mesh->ebo)
			glDrawElements(mesh->mode, mesh->drawSize, GL_UNSIGNED_INT, 0);
		else
			glDrawArrays(mesh->mode, 0, mesh->drawSize);
	} else {
		glBindVertexArray(this.defaultMesh.vao);
		if (this.defaultMesh.ebo)
			glDrawElements(this.defaultMesh.mode, this.defaultMesh.drawSize, GL_UNSIGNED_INT, 0);
		else
			glDrawArrays(this.defaultMesh.mode, 0, this.defaultMesh.drawSize);
	}
	glBindVertexArray(GL_INIT_DEFAULT_MESH.vao);
}

void gl_mesh_delete(gl_mesh* mesh) {
	if (mesh->vao != GL_INIT_DEFAULT_MESH.vao)
		glDeleteVertexArrays(1, &mesh->vao);
	if (mesh->vbo != GL_INIT_DEFAULT_MESH.vbo)
		glDeleteBuffers(1, &mesh->vbo);
	if (mesh->ebo != GL_INIT_DEFAULT_MESH.ebo)
		glDeleteBuffers(1, &mesh->ebo);
	*mesh = GL_INIT_DEFAULT_MESH;
}

gl_tex gl_texture_create(gl_texformat format, size2d_t size) {
	gl_tex texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	
	glTexImage2D(GL_TEXTURE_2D, 0, gl_texformat_lookup[format].internalFormat, size.width, size.height, 0, gl_texformat_lookup[format].format, gl_texformat_lookup[format].type, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glBindTexture(GL_TEXTURE_2D, GL_INIT_DEFAULT_TEX);
	return texture;
}

gl_tex gl_textureArray_create(gl_texformat format, size3d_t size) {
	gl_tex texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
	
	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, gl_texformat_lookup[format].internalFormat, size.width, size.height, size.depth , 0, gl_texformat_lookup[format].format, gl_texformat_lookup[format].type, NULL);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glBindTexture(GL_TEXTURE_2D_ARRAY, GL_INIT_DEFAULT_TEX);
	return texture;
}

int gl_texture_check(gl_tex* texture) {
	if (*texture == GL_INIT_DEFAULT_TEX)
		return 0;
	return 1;
}

void gl_texture_update(gl_texformat format, gl_tex* texture, size2d_t size, void* data) {
	glBindTexture(GL_TEXTURE_2D, *texture);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, size.width, size.height, gl_texformat_lookup[format].format, gl_texformat_lookup[format].type, data);
	glBindTexture(GL_TEXTURE_2D, GL_INIT_DEFAULT_TEX);
}

void gl_textureArray_update(gl_texformat format, gl_tex* texture, size3d_t size, void* data) {
	glBindTexture(GL_TEXTURE_2D_ARRAY, *texture);
	glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, size.depth, size.width, size.height, 1, gl_texformat_lookup[format].format, gl_texformat_lookup[format].type, data);
	glBindTexture(GL_TEXTURE_2D_ARRAY, GL_INIT_DEFAULT_TEX);
}

void gl_texture_bind(gl_tex* texture, gl_param paramId, unsigned int unit) {
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(GL_TEXTURE_2D, *texture);
	glUniform1i(paramId, unit);
}

void gl_textureArray_bind(gl_tex* texture, gl_param paramId, unsigned int unit) {
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(GL_TEXTURE_2D_ARRAY, *texture);
	glUniform1i(paramId, unit);
}

void gl_texture_delete(gl_tex* texture) {
	if (*texture != GL_INIT_DEFAULT_TEX)
		glDeleteTextures(1, texture);
	*texture = GL_INIT_DEFAULT_TEX;
}

gl_pbo gl_pixelBuffer_create(size_t size) {
	gl_pbo pbo;
	glGenBuffers(1, &pbo);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
	glBufferData(GL_PIXEL_UNPACK_BUFFER, size, NULL, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, GL_INIT_DEFAULT_PBO);
	return pbo;
}

int gl_pixelBuffer_check(gl_pbo* pbo) {
	if (*pbo == GL_INIT_DEFAULT_FBO)
		return 0;
	return 1;
}

void* gl_pixelBuffer_updateStart(gl_pbo* pbo, size_t size) {
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, *pbo);
	return glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, size, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
}

void gl_pixelBuffer_updateFinish() {
	glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, GL_INIT_DEFAULT_PBO);
}

void gl_pixelBuffer_updateToTexture(gl_texformat format, gl_pbo* pbo, gl_tex* texture, size2d_t size) {
	glBindTexture(GL_TEXTURE_2D, *texture);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, *pbo);

	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, size.width, size.height, gl_texformat_lookup[format].format, gl_texformat_lookup[format].type, 0);
	
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, GL_INIT_DEFAULT_PBO);
	glBindTexture(GL_TEXTURE_2D, GL_INIT_DEFAULT_TEX);
}

void gl_pixelBuffer_delete(gl_pbo* pbo) {
	if (*pbo != GL_INIT_DEFAULT_PBO)
		glDeleteBuffers(1, pbo);
	*pbo = GL_INIT_DEFAULT_PBO;
}

gl_fb gl_frameBuffer_create(gl_texformat format, size2d_t size) {
	gl_fb buffer = GL_INIT_DEFAULT_FB;

	glGenFramebuffers(1, &buffer.frame);
	glBindFramebuffer(GL_FRAMEBUFFER, buffer.frame);

	buffer.texture = gl_texture_create(format, size);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, buffer.texture, 0);

	return buffer;
}

int gl_frameBuffer_check(gl_fb* fb) {
	if (fb->frame == GL_INIT_DEFAULT_FB.frame)
		return 0;
	if (fb->texture == GL_INIT_DEFAULT_FB.texture)
		return 0;
	return 1;
}

void gl_frameBuffer_bind(gl_fb* fb, size2d_t size, int clear) {
	glBindFramebuffer(GL_FRAMEBUFFER, fb->frame);

	if (size.width || size.height) {
		glViewport(0, 0, size.width, size.height);
	}

	if (clear) {
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
	}
}

void gl_frameBuffer_download(gl_fb* fb, size2d_t size, void* dest) {
	glBindFramebuffer(GL_FRAMEBUFFER, fb->frame);
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glReadPixels(0, 0, size.width, size.height, GL_RGBA, GL_FLOAT, dest);
}

vec4 gl_frameBuffer_getPixel(gl_fb* fb, size2d_t where) {
	vec4 d;
	glBindFramebuffer(GL_FRAMEBUFFER, fb->frame);
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glReadPixels(where.x, where.y, 1, 1, GL_RGBA, GL_FLOAT, &d);
	return d;
}

void gl_frameBuffer_delete(gl_fb* fb) {
	if (fb->frame != GL_INIT_DEFAULT_FB.frame)
		glDeleteFramebuffers(1, &fb->frame);
	if (fb->texture != GL_INIT_DEFAULT_FB.texture)
		gl_texture_delete(&fb->texture);
	*fb = GL_INIT_DEFAULT_FB;
}

void gl_fsync() {
	glFinish();
}
void gl_rsync() {
	glFlush();
}

gl_synch gl_synchSet() {
	return gl_synchSet_ptr();
}
gl_synch_statue gl_synchWait(gl_synch s, uint64_t timeout) {
	return gl_synchWait_ptr(s, timeout);
}
void gl_synchDelete(gl_synch s) {
	gl_synchDelete_ptr(s);
}