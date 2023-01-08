#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <GL/glew.h>
#include <GL/glfw3.h>

#include "gl.h"

#if !defined(__arm__) && !defined(__aarch64__)
	#define SHADER_HEADER_SUPPORTTEXT
#endif

/* == Window and driver management ========================================================== */

GLFWwindow* window = NULL; //Display window object

void __gl_windowCloseCallback(GLFWwindow* window); //Event callback when window closed by user (X button or kill)
void __gl_glfwErrorCallback(int code, const char* desc); //GLFW error log
void GLAPIENTRY __gl_glErrorCallback(GLenum src, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam); //OpenGL log

/** Print log to log stream. 
 * @param format A C printf-style format string
 * @param ... A list of arguments
 */
#define __gl_log(format, ...) (fprintf(stderr, "[GL] Log:\t"format"\n" __VA_OPT__(,) __VA_ARGS__))

/** Print error log to error log stream. 
 * @param format A C printf-style format string
 * @param ... A list of arguments
 */
#define __gl_elog(format, ...) (fprintf(stderr, "[GL] Err:\t"format"\n" __VA_OPT__(,) __VA_ARGS__))

int gl_init(gl_config config) {
	#ifdef VERBOSE
		__gl_log("Init OpenGL");
	#endif

	/* init GLFW */
	if (!glfwInit()) {
		#ifdef VERBOSE
			__gl_elog("\tFail to init GLFW");
		#endif
		gl_destroy();
		return 0;
	}

	/* Start and config OpenGL, init window */
	glfwWindowHint(GLFW_CLIENT_API, config.gles ? GLFW_OPENGL_ES_API : GLFW_OPENGL_API);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, config.vMajor);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, config.vMinor);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
//	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	#ifdef VERBOSE
		glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
	#endif
	window = glfwCreateWindow(config.winWidth, config.winHeight, config.winName, NULL, NULL);
	if (!window){
		#ifdef VERBOSE
			__gl_elog("\tFail to open window");
		#endif
		gl_destroy();
		return 0;
	}
	glfwMakeContextCurrent(window);
	glfwSwapInterval(0);

	/* Init GLEW */
	glewExperimental = GL_TRUE;
	GLenum glewInitError = glewInit();
	if (glewInitError != GLEW_OK) {
		#ifdef VERBOSE
			__gl_elog("\tFail init GLEW: %s", glewGetErrorString(glewInitError));
		#endif
		gl_destroy();
		return 0;
	}

	/* Driver and hardware info */
	#ifdef VERBOSE
		__gl_log("OpenGL driver and hardware info:");
		int textureSize, textureSizeArray, textureSize3d, textureImageUnit, textureImageUnitVertex;
		glGetIntegerv(GL_MAX_TEXTURE_SIZE, &textureSize);
		glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &textureSizeArray);
		glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &textureSize3d);
		glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &textureImageUnit);
		glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &textureImageUnitVertex);
		__gl_log("\t- Vendor:                   %s", glGetString(GL_VENDOR));
		__gl_log("\t- Renderer:                 %s", glGetString(GL_RENDERER));
		__gl_log("\t- Version:                  %s", glGetString(GL_VERSION));
		__gl_log("\t- Shader language version:  %s", glGetString(GL_SHADING_LANGUAGE_VERSION));
		__gl_log("\t- Max texture size:         %d", textureSize);
		__gl_log("\t- Max texture layers:       %d", textureSizeArray);
		__gl_log("\t- Max 3D texture size:      %d", textureSize3d);
		__gl_log("\t- Max texture Units:        %d", textureImageUnit);
		__gl_log("\t- Max Vertex texture units: %d", textureImageUnitVertex);
	#endif

	/* Event control - callback */
	glfwSetWindowCloseCallback(window, __gl_windowCloseCallback);
	glfwSetErrorCallback(__gl_glfwErrorCallback);
	glDebugMessageCallback(__gl_glErrorCallback, NULL);

	/* OpenGL config */
//	glEnable(GL_PROGRAM_POINT_SIZE); glPointSize(10.0f);
//	glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS);
//	glEnable(GL_CULL_FACE); glCullFace(GL_BACK); glFrontFace(GL_CCW);
	glfwSetCursor(window, glfwCreateStandardCursor(GLFW_CROSSHAIR_CURSOR));

	return 1;
}

void* gl_getWindow() {
	return (void*)window;
}

void gl_drawStart() {
	glfwPollEvents();
}

gl_winsizeNcursor gl_getWinsizeCursor() {
	gl_winsizeNcursor x;
	glfwGetCursorPos(window, x.curPos, x.curPos+1); //In fact, this call returns cursor pos respect to window
	glfwGetWindowSize(window, x.winsize, x.winsize+1);
	glfwGetFramebufferSize(window, x.framesize, x.framesize+1); //Window size and framebuffer size may differ if DPI is not 1
	x.curPosWin[0] = x.curPos[0];	x.curPosWin[1] = x.curPos[1];
	x.curPos[0] /= x.winsize[0];	x.curPos[1] /= x.winsize[1];
	x.curPosFrame[0] = x.curPos[0] * x.framesize[0];
	x.curPosFrame[1] = x.curPos[1] * x.framesize[1];
	return x;
}


void gl_setViewport(const unsigned int offset[static 2], const unsigned int size[static 2]) {
	glViewport(offset[0], offset[1], size[0], size[1]);
}

void gl_drawEnd(const char* const title) {
	if (title)
		glfwSetWindowTitle(window, title);
	
	glfwSwapBuffers(window);
}

int gl_close(const int close) {
	if (close == 0) {
		#ifdef VERBOSE
			__gl_log("Request to keep window");
		#endif
		glfwSetWindowShouldClose(window, 0);
		return 0;
	} else if (close > 0) {
		#ifdef VERBOSE
			__gl_log("Request to close window");
		#endif
		glfwSetWindowShouldClose(window, 1);
		return 1;
	} else {
		return glfwWindowShouldClose(window);
	}
}

void gl_destroy() {
	#ifdef VERBOSE
		__gl_log("Destroy OpenGL");
	#endif
	gl_close(1);
	glfwTerminate();
}

void gl_lineMode(const unsigned int weight) {
	if (weight) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glLineWidth(weight);
	} else {
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}
}

void gl_fsync() {
	glFinish();
}

void gl_rsync() {
	glFlush();
}

gl_synch gl_synchSet() {
	gl_synch s = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	return s;
}
gl_synch_status gl_synchWait(const gl_synch s, uint64_t const timeout) {
	gl_synch_status statue;
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
void gl_synchDelete(const gl_synch s) {
	glDeleteSync(s);
}

void __gl_windowCloseCallback(GLFWwindow* window) {
	__gl_log("Window close event fired");
	glfwSetWindowShouldClose(window, 1);
}
void __gl_glfwErrorCallback(int code, const char* desc) {
	__gl_elog("GLFW error %d: %s", code, desc);
}
void GLAPIENTRY __gl_glErrorCallback(GLenum src, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
	if (type == GL_DEBUG_TYPE_ERROR)
		__gl_elog("GL Driver: type %x, severity %x, message: %s", type, severity, message);
	else
		__gl_log("GL Driver: type %x, severity %x, message: %s", type, severity, message);
}

/* == Shader and shader param data types, UBO =============================================== */

char* shaderCommonHeader = ""; //Default is empty, with 0-terminator, NOT NULL

/** Load content of a shader from file into memory. 
 * This function append a null terminator to indicate the end of the shader section. 
 * If length is not NULL, it can be used to get the length of the content (without the appended null terminator). 
 * Length will be 0 if cannot open file, be negative if cannot measure file size. 
 * In case the program cannot allocate memory for content buffer, length will be positive (the true size of the content), but the function will return NULL. 
 * Use __gl_unloadFileFromMemory() to free the content memory when no longer need. 
 * @param filename Directory to the file
 * @param length Pass-by-reference, if not NULL, return the length of the file (without the appended null terminator). 
 * @return Address of content buffer. return NULL if fail
 */
char* __gl_loadFileToMemory(const char* const filename, long int* const length);

/** Free the buffer allocated by file reading.
 * @param buffer Buffer returned by __gl_loadFileToMemory()
 */
void __gl_unloadFileFromMemory(char* const buffer);

void gl_program_setCommonHeader(const char* const header) {
	shaderCommonHeader = (char*)header;
}

gl_program gl_program_create(const gl_programSrc* const srcs, gl_programArg* const args) {
	const int shaderTypeLookup[] = {GL_VERTEX_SHADER, GL_FRAGMENT_SHADER, GL_GEOMETRY_SHADER}; //Associated with enum GL_ProgramSourceCodeType
	const char* shaderTypeDescription[] = {"vertex", "fragment", "geometry (optional)"};

	#ifdef VERBOSE
		__gl_log("Init shader program");
	#endif

	uint32_t shaderTypePresent = 0x00000000;

	/* Check function param */ {
		for (gl_programSrc* s = (gl_programSrc*)srcs; s->str; s++) {
			if (s->type >= gl_programSrcType_placeholderEnd || s->type < 0) {
				__gl_elog("Bad shader code type");
				return GL_INIT_DEFAULT_PROGRAM;
			}
			if (s->loc >= gl_programSrcLoc_placeholderEnd || s->loc < 0) {
				__gl_elog("Bad shader code source");
				return GL_INIT_DEFAULT_PROGRAM;
			}
			shaderTypePresent |= 1 << s->type;
		}
		for (gl_programArg* a = args; a->name; a++) {
			if (a->type >= gl_programArgType_placeholderEnd || a->type < 0) {
				__gl_elog("Bad shader program argument type");
				return GL_INIT_DEFAULT_PROGRAM;
			}
		}
	}

	/* Prepare buffer, shader and program object */
	GLuint program = 0;
	struct {
		char* buffer;
		unsigned int count;
		unsigned int size; //4G (uint_max) is enough for a shader src
	} shaderBuffer[gl_programSrcType_placeholderEnd];
	for (unsigned int type = 0; type < gl_programSrcType_placeholderEnd; type++) {
		shaderBuffer[type].buffer = NULL;
		shaderBuffer[type].count = 0;
		shaderBuffer[type].size = 0;
	}
	//Destroy all shaders and buffers
	#define destroy_all_shader_buffer() { \
		for (unsigned int i = 0; i < gl_programSrcType_placeholderEnd; i++) \
			free(shaderBuffer[i].buffer); \
	}

	/* Write shader header */
	for (unsigned int type = 0; type < gl_programSrcType_placeholderEnd; type++) {
		if (shaderTypePresent & (1 << type)) {
			long int len = strlen(shaderCommonHeader);
			if (!( shaderBuffer[type].buffer = malloc(len + 1) )) {
				__gl_elog("Fail to write code for %s shader buffer, section header", shaderTypeDescription[type]);
				destroy_all_shader_buffer();
				return GL_INIT_DEFAULT_PROGRAM;
			}
			memcpy(shaderBuffer[type].buffer, shaderCommonHeader, len);
			shaderBuffer[type].buffer[len] = '\n';
			shaderBuffer[type].size += len + 1;
		}
	}

	/* Load shader source code into buffer */
	for (gl_programSrc* s = (gl_programSrc*)srcs; s->str; s++) {
		const gl_programSrcType type = s->type;
		const char* str = s->str;
		long int len;
		char* code;

		#ifdef VERBOSE
			if (s->loc == gl_programSrcLoc_mem)
				__gl_log("  - Load section %u of %s shader, from memory @ %p", shaderBuffer[type].count, shaderTypeDescription[type], str);
			else
				__gl_log("  - Load section %u of %s shader, from file: %s", shaderBuffer[type].count, shaderTypeDescription[type], str);
		#endif

		/* Write shader code section header to buffer */
		char header[192] = "#line 1"; //Default macro
		#ifdef SHADER_HEADER_SUPPORTTEXT
			const char* headerTemplate[gl_programSrcLoc_placeholderEnd] = {
				"#line 1 \"Shader %s shader, section %u, from memory @ %p \"", //Do not put new-line here
				"#line 1 \"Shader %s shader, section %u, from file: %s \""
			};
			snprintf(header, sizeof(header), headerTemplate[s->loc], shaderTypeDescription[type], shaderBuffer[type].count, str);
		#endif
		len = strlen(header);
		/* Note:
		* snprintf() has guarantee size of header string length will not excess sizeof(header), so we don't need use any function with size check later (e.g. strnlen). 
		* It is possible the actual header content excess header size. Which means, the tail new-line in header will be ignored. 
		* In this case, we add the new-line character exclusively after write the header. 
		* strlen() returns the length of header without the null-terminator. We allocate memory size of strlen()+1, and then put new-line at the last memory space. 
		*/
		if (!( shaderBuffer[type].buffer = realloc(shaderBuffer[type].buffer, shaderBuffer[type].size + len + 1) )) {
			__gl_elog("Fail to write header for %s shader buffer, section %u", shaderTypeDescription[type], shaderBuffer[type].count);
			destroy_all_shader_buffer();
			return GL_INIT_DEFAULT_PROGRAM;
		}
		memcpy(shaderBuffer[type].buffer + shaderBuffer[type].size /* Address after resize, offset at the end of previous content */, header, len);
		shaderBuffer[type].buffer[ shaderBuffer[type].size + len ] = '\n';
		shaderBuffer[type].size += len + 1; /* New buffer size */

		/* Load shader code content */
		if (s->loc == gl_programSrcLoc_mem) {
			code = (char*)str; //Copy the pointer to code, no memory allocation here
			len = strlen(code);
		} else {
			code = __gl_loadFileToMemory(str, &len); //Read file into memory space code --> Copy content from code to buffer --> Free memory space code
			if (!code) {
				if (!len)
					__gl_elog("Fail to load shader from file %s: Cannot open file", str);
				else if (len < 0)
					__gl_elog("Fail to load shader from file %s: Cannot get file length", str);
				else
					__gl_elog("Fail to load shader from file %s: Cannot create buffer", str);
				destroy_all_shader_buffer();
				return GL_INIT_DEFAULT_PROGRAM;
			}
		}

		/* Write shader code content to buffer */
		if (!( shaderBuffer[type].buffer = realloc(shaderBuffer[type].buffer, shaderBuffer[type].size + len + 1) )) {
			__gl_elog("Fail to write code for %s shader buffer, section %u", shaderTypeDescription[type], shaderBuffer[type].count);
			destroy_all_shader_buffer();
			return GL_INIT_DEFAULT_PROGRAM;
		}
		memcpy(shaderBuffer[type].buffer + shaderBuffer[type].size, code, len);
		shaderBuffer[type].buffer[ shaderBuffer[type].size + len ] = '\n'; //Append a new-line in case the application/file doesn't provide one
		shaderBuffer[type].size += len + 1;
		
		shaderBuffer[type].count++;

		/* Free temp buffer for code content loading */
		if (s->loc == gl_programSrcLoc_file) {
			__gl_unloadFileFromMemory(code);
		}
	}

	/* Compile shaders and link program */ {
		GLuint shader, msgLength;
		GLint status;
		if (!(program = glCreateProgram())) {
			__gl_elog("Cannot create shader program");
			destroy_all_shader_buffer();
			return GL_INIT_DEFAULT_PROGRAM;
		}
		#ifdef VERBOSE
			__gl_log("  - Create shader program, id = %u", program);
		#endif
		for (unsigned int type = 0; type < gl_programSrcType_placeholderEnd; type++) {
			if (shaderBuffer[type].buffer) { //Compile only if tis shader type is presented
				if (!( shader = glCreateShader(shaderTypeLookup[type]) )) {
					__gl_elog("Cannot create %s shader", shaderTypeDescription[type]);
					glDeleteProgram(program);
					destroy_all_shader_buffer();
					return GL_INIT_DEFAULT_PROGRAM;
				}
				#ifdef VERBOSE
					__gl_log("  - Create and compile shader, id = %u: %s shader", shader, shaderTypeDescription[type]);
				#endif
				const GLchar* code[1] = {shaderBuffer[type].buffer};
				const GLint size[1] = {shaderBuffer[type].size};
				glShaderSource(shader, 1, code, size);
				glCompileShader(shader);
				glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
				if (!status) {
					glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &msgLength);
					char msg[msgLength];
					glGetShaderInfoLog(shader, msgLength, NULL, msg);
					__gl_elog("Error in %s shader:\n%s", shaderTypeDescription[type], msg);
					glDeleteShader(shader); //Delete current shader
					glDeleteProgram(program); //Delete program and all previous shaders, previous shaders have attached and flag for delete
					destroy_all_shader_buffer();
					return GL_INIT_DEFAULT_PROGRAM;
				}
				glAttachShader(program, shader); //Attach current shader, so we can flag them delete for automatical delete
				glDeleteShader(shader); //Shader compiled and attached, flag the shader for delete
			}
		}
		glLinkProgram(program);
		glGetProgramiv(program, GL_LINK_STATUS, &status);
		if (!status) {
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &msgLength);
			char msg[msgLength];
			glGetProgramInfoLog(program, msgLength, NULL, msg);
			__gl_elog("Shader program link fail:\n%s", msg);
			glDeleteProgram(program);
			destroy_all_shader_buffer();
			return GL_INIT_DEFAULT_PROGRAM;
		}
		destroy_all_shader_buffer(); //Shader code compiled, buffer no longer need
	}

	/* Bind arguments */ {
		for (gl_programArg* a = args; a->name; a++) {
			switch (a->type) {
				case gl_programArgType_normal:
					a->id = glGetUniformLocation(program, a->name);
					break;
				case gl_programArgType_ubo:
					a->id = glGetUniformBlockIndex(program, a->name);
					break;
			}
			if (a->id == -1) {
				__gl_elog("Shader program argument bind fail: Argument %s is not in shader code", a->name);
				glDeleteProgram(program);
				return GL_INIT_DEFAULT_PROGRAM;
			}
			#ifdef VERBOSE
				__gl_log("  - Shader program argument \"%s\" bind to %d", a->name, a->id);
			#endif
		}
	}

	#ifdef VERBOSE
		GLuint binSize;
		glGetProgramiv(program, GL_PROGRAM_BINARY_LENGTH, &binSize);
		__gl_log("  - Shader program (%u) create success, size %u", program, binSize);
	#endif
	return program;
}

int gl_program_check(const gl_program* const program) {
	return *program != GL_INIT_DEFAULT_PROGRAM;
}

void gl_program_use(const gl_program* const program) {
	glUseProgram(*program);
}

void gl_program_setParam(const gl_param paramId, const unsigned int length, const gl_datatype type, const void* const data) {
	if (length - 1 > 3) { //If pass 0, 0-1 get 0xFFFFFFFF
		__gl_elog("Param set fail: GL supports vector size 1 to 4 only");
		return;
	}

	if (type == gl_datatype_int) {
		const int* d = data;
		if (length == 4)
			glUniform4i(paramId, d[0], d[1], d[2], d[3]);
		else if (length == 3)
			glUniform3i(paramId, d[0], d[1], d[2]);
		else if (length == 2)
			glUniform2i(paramId, d[0], d[1]);
		else
			glUniform1i(paramId, d[0]);
	} else if (type == gl_datatype_uint) {
		const unsigned int* d = data;
		if (length == 4)
			glUniform4ui(paramId, d[0], d[1], d[2], d[3]);
		else if (length == 3)
			glUniform3ui(paramId, d[0], d[1], d[2]);
		else if (length == 2)
			glUniform2ui(paramId, d[0], d[1]);
		else
			glUniform1ui(paramId, d[0]);
	} else if (type == gl_datatype_float) {
		const float* d = data;
		if (length == 4)
			glUniform4f(paramId, d[0], d[1], d[2], d[3]);
		else if (length == 3)
			glUniform3f(paramId, d[0], d[1], d[2]);
		else if (length == 2)
			glUniform2f(paramId, d[0], d[1]);
		else
			glUniform1f(paramId, d[0]);
	} else
		__gl_elog("Param set fail: GL supports date type int, uint and float only");
}

void gl_program_delete(gl_program* const program) {
	glDeleteProgram(*program);
	*program = GL_INIT_DEFAULT_PROGRAM;
}

gl_ubo gl_uniformBuffer_create(const unsigned int bindingPoint, const unsigned int size, const gl_usage usage) {
	const GLenum usageLookup[] = {GL_STREAM_DRAW, GL_STATIC_DRAW, GL_DYNAMIC_DRAW};
	if (usage < 0 || usage >= gl_usage_placeholderEnd)
		return GL_INIT_DEFAULT_UBO;

	gl_ubo ubo = GL_INIT_DEFAULT_UBO;

	glGenBuffers(1, &ubo);
	glBindBuffer(GL_UNIFORM_BUFFER, ubo);
	glBufferData(GL_UNIFORM_BUFFER, size, NULL, usageLookup[usage]);
	glBindBufferBase(GL_UNIFORM_BUFFER, bindingPoint, ubo);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	return ubo;
}

int gl_uniformBuffer_check(const gl_ubo* const ubo) {
	return *ubo != GL_INIT_DEFAULT_UBO;
}

void gl_uniformBuffer_bindShader(const unsigned int bindingPoint, const gl_program* const program, const gl_param paramId) {
	glUniformBlockBinding(*program, paramId, bindingPoint);
}

void gl_uniformBuffer_update(const gl_ubo* const ubo, const unsigned int start, const unsigned int len, const void* const data) {
	glBindBuffer(GL_UNIFORM_BUFFER, *ubo);
	glBufferSubData(GL_UNIFORM_BUFFER, start, len, data);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void gl_unifromBuffer_delete(gl_ubo* const ubo) {
	glDeleteBuffers(1, ubo);
	*ubo = GL_INIT_DEFAULT_UBO;
}

char* __gl_loadFileToMemory(const char* const filename, long int* const length) {
	long int len = 0; //Default length = 0, fail to open file
	if (length)
		*length = len;

	FILE* fp = fopen(filename, "r");
	if (!fp)
		return NULL; //Error: Cannot open file, length will be 0
	
	fseek(fp, 0, SEEK_END);
	len = ftell(fp);
	rewind(fp);
	if (length)
		*length = len; //File open success, try get length

	if (len < 0) {
		fclose(fp);
		return NULL; //Error: Cannot get file length, length set to negative by ftell()
	}
	
	char* content = malloc(len + 1);
	if (!content) {
		fclose(fp);
		return NULL; //Error: Cannot allocate memory, length is now positive (the length of file)
	}

	int whatever = fread(content, 1, len, fp);
	content[len] = '\0'; //Append null terminator
	fclose(fp);
	return content;
}
void __gl_unloadFileFromMemory(char* const buffer) {
	free(buffer);
}

/* == Mesh (vertices) ======================================================================= */

gl_instance gl_instance_create(const unsigned int size, const gl_usage usage) {
	const GLenum usageLookup[] = {GL_STREAM_DRAW, GL_STATIC_DRAW, GL_DYNAMIC_DRAW};
	GLuint instance;
	glGenBuffers(1, &instance);
	glBindBuffer(GL_ARRAY_BUFFER, instance);
	glBufferData(GL_ARRAY_BUFFER, size, NULL, usageLookup[usage]);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	return instance;
}

int gl_instance_check(const gl_instance* const instance) {
	return *instance != GL_INIT_DEFAULT_INSTANCE;
}

void gl_instance_update(const gl_instance* const instance, const unsigned int start, const unsigned int len, const void* const data) {
	glBindBuffer(GL_ARRAY_BUFFER, *instance);
	glBufferSubData(GL_ARRAY_BUFFER, start, len, data);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void gl_instance_delete(gl_instance* const instance) {
	glDeleteBuffers(1, instance);
	*instance = GL_INIT_DEFAULT_INSTANCE;
}

gl_mesh gl_mesh_create(
	const unsigned int verticesCount,
	const unsigned int indicesCount,
	const gl_index_t* const elementsSize,
	const gl_index_t* const instanceSize,
	const gl_vertex_t* const vertices,
	const gl_index_t* const indices,
	const gl_instance* const instance,
	const gl_meshmode mode,
	const gl_usage usage
) {
	const GLenum usageLookup[] = {GL_STREAM_DRAW, GL_STATIC_DRAW, GL_DYNAMIC_DRAW};
	gl_mesh mesh = GL_INIT_DEFAULT_MESH;

	if (usage < 0 || usage >= gl_usage_placeholderEnd)
		return GL_INIT_DEFAULT_MESH;
	
	mesh.drawSize = indicesCount ? indicesCount : verticesCount;
	switch(mode) {
		case gl_meshmode_points:
			mesh.mode = GL_POINTS;
			break;
		case gl_meshmode_lines:
			mesh.mode = GL_LINES;
			break;
		case gl_meshmode_triangles:
			mesh.mode = GL_TRIANGLES;
			break;
		case gl_meshmode_triangleFan:
			mesh.mode = GL_TRIANGLE_FAN;
			break;
		case gl_meshmode_triangleStrip:
			mesh.mode =  GL_TRIANGLE_STRIP;
			break;
		default:
			return GL_INIT_DEFAULT_MESH;
	}
	
	unsigned int vertexCount = 0, instanceCount = 0; //Get number of elements in one vertex
	for (const gl_index_t* x = elementsSize; *x; x++) {
		vertexCount += *x;
	}
	if (instanceSize) {
		for (const gl_index_t* x = instanceSize; *x; x++) {
			instanceCount += *x;
		}
	}

	glGenVertexArrays(1, &mesh.vao);
	glGenBuffers(1, &mesh.vbo);
	if (indicesCount)
		glGenBuffers(1, &mesh.ebo);
	
	glBindVertexArray(mesh.vao);
	glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
	glBufferData(GL_ARRAY_BUFFER, verticesCount * vertexCount * sizeof(gl_vertex_t), vertices, usageLookup[usage]);
	if (indicesCount) {
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indicesCount * sizeof(gl_index_t), indices, usageLookup[usage]);
	}

	gl_index_t aIdx = 0; //Layout in shader program
	GLuint eStep = 0, iStep = 0; //Accumulate offset of current element in vertex
	for (const gl_index_t* eSize = elementsSize; *eSize; eSize++) { //Get size of each element, set attribute
		glEnableVertexAttribArray(aIdx);
		glVertexAttribPointer(aIdx, *eSize, GL_FLOAT, GL_FALSE, vertexCount * sizeof(float), (GLvoid*)(eStep * sizeof(float)));
		eStep += *eSize;
		aIdx++;
	}
	if (instanceSize) {
		glBindBuffer(GL_ARRAY_BUFFER, *instance); /* Must bind array buffer to instance buffer when set instance data */
		for (const gl_index_t* iSize = instanceSize; *iSize; iSize++) { //Get size of each element, set attribute
			glEnableVertexAttribArray(aIdx);
			glVertexAttribPointer(aIdx, *iSize, GL_FLOAT, GL_FALSE, instanceCount * sizeof(float), (GLvoid*)(iStep * sizeof(float)));
			glVertexAttribDivisor(aIdx, 1);
			iStep += *iSize;
			aIdx++;
		}
		mesh.ibo = *instance;
	}
	
	glBindBuffer(GL_ARRAY_BUFFER, GL_INIT_DEFAULT_MESH.vbo);
	glBindVertexArray(GL_INIT_DEFAULT_MESH.vao);
	return mesh;
}

int gl_mesh_check(const gl_mesh* const mesh) {
	return mesh->vao != GL_INIT_DEFAULT_MESH.vao && mesh->vbo != GL_INIT_DEFAULT_MESH.vbo;
}

void gl_mesh_update(const gl_mesh* const mesh, const gl_vertex_t* const vertices, const gl_index_t* const indices, const unsigned int offCnt[static 4]) {
	const unsigned int vOff = offCnt[0], vCnt = offCnt[1], iOff = offCnt[2], iCnt = offCnt[3];
	glBindVertexArray(mesh->vao);
	
	if ( vCnt && vertices ) {
		glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
		glBufferSubData(GL_ARRAY_BUFFER, vOff * sizeof(gl_vertex_t), vCnt * sizeof(gl_vertex_t), vertices);
	}

	if ( mesh->ebo != GL_INIT_DEFAULT_MESH.ebo && iCnt && indices ) {
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ebo);
		glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, iOff * sizeof(gl_index_t), iCnt * sizeof(gl_index_t), indices);
	}
	
	glBindBuffer(GL_ARRAY_BUFFER, GL_INIT_DEFAULT_MESH.vbo);
	glBindVertexArray(GL_INIT_DEFAULT_MESH.vao);
}

void gl_mesh_draw(const gl_mesh* const mesh, const unsigned int vSize, const unsigned int iSize) {
	glBindVertexArray(mesh->vao);
	if (mesh->ibo) {
		if (mesh->ebo)
			glDrawElementsInstanced(mesh->mode, vSize ? vSize : mesh->drawSize, GL_UNSIGNED_INT, 0, iSize);
		else
			glDrawArraysInstanced(mesh->mode, 0, vSize ? vSize : mesh->drawSize, iSize);
	} else {
		if (mesh->ebo)
			glDrawElements(mesh->mode, vSize ? vSize : mesh->drawSize, GL_UNSIGNED_INT, 0);
		else
			glDrawArrays(mesh->mode, 0, vSize ? vSize : mesh->drawSize);
	}
	glBindVertexArray(GL_INIT_DEFAULT_MESH.vao);
}

void gl_mesh_delete(gl_mesh* const mesh) {
	glDeleteVertexArrays(1, &mesh->vao);
	glDeleteBuffers(1, &mesh->vbo);
	glDeleteBuffers(1, &mesh->ebo);
	*mesh = GL_INIT_DEFAULT_MESH;
}

/* == Texture, PBO for texture transfer and FBO for off-screen rendering ==================== */

//OpenGL texture format lookup table
const struct {
	gl_texformat eFormat;
	GLint internalFormat;
	GLenum format;
	GLenum type;
} __gl_texformat_lookup[gl_texformat_placeholderEnd] = {
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
	{gl_texformat_RGBA32UI,		GL_RGBA32UI,	GL_RGBA_INTEGER,	GL_UNSIGNED_INT		},
	{gl_texformat_d16,	GL_DEPTH_COMPONENT16,	GL_DEPTH_COMPONENT,	GL_UNSIGNED_INT		},
	{gl_texformat_d24,	GL_DEPTH_COMPONENT24,	GL_DEPTH_COMPONENT,	GL_UNSIGNED_INT		},
	{gl_texformat_d32f,	GL_DEPTH_COMPONENT32F,	GL_DEPTH_COMPONENT,	GL_FLOAT		},
	{gl_texformat_d24s8,	GL_DEPTH24_STENCIL8,	GL_DEPTH_STENCIL,	GL_UNSIGNED_INT_24_8	},
	{gl_texformat_d32fs8,	GL_DEPTH32F_STENCIL8,	GL_DEPTH_STENCIL,	GL_FLOAT_32_UNSIGNED_INT_24_8_REV},
	{gl_texformat_s8,	GL_STENCIL_INDEX8,	GL_STENCIL_INDEX,	GL_UNSIGNED_BYTE	},
};

gl_tex gl_texture_create(const gl_texformat format, const gl_textype type, const unsigned int size[static 3]) {
	if (format < 0 || format >= gl_texformat_placeholderEnd)
		return GL_INIT_DEFAULT_TEX;
	if (type < 0 || type > gl_textype_placeholderEnd)
		return GL_INIT_DEFAULT_TEX;

	GLuint tex;
	glGenTextures(1, &tex);

	switch (type) {
		case gl_textype_2d:
			glBindTexture(GL_TEXTURE_2D, tex);
			glTexImage2D(
				GL_TEXTURE_2D, 0, __gl_texformat_lookup[format].internalFormat,
				size[0], size[1],
				0, __gl_texformat_lookup[format].format, __gl_texformat_lookup[format].type, NULL
			);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); //GL_LINEAR
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glBindTexture(GL_TEXTURE_2D, GL_INIT_DEFAULT_TEX.texture);
			break;
		case gl_textype_2dArray:
			glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
			glTexImage3D(
				GL_TEXTURE_2D_ARRAY, 0, __gl_texformat_lookup[format].internalFormat,
				size[0], size[1], size[2],
				0, __gl_texformat_lookup[format].format, __gl_texformat_lookup[format].type, NULL
			);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glBindTexture(GL_TEXTURE_2D_ARRAY, GL_INIT_DEFAULT_TEX.texture);
			break;
		case gl_textype_3d:
			glBindTexture(GL_TEXTURE_3D, tex);
			glTexImage3D(
				GL_TEXTURE_3D, 0, __gl_texformat_lookup[format].internalFormat,
				size[0], size[1], size[2],
				0, __gl_texformat_lookup[format].format, __gl_texformat_lookup[format].type, NULL
			);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
			glBindTexture(GL_TEXTURE_3D, GL_INIT_DEFAULT_TEX.texture);
			break;
	}

	return (gl_tex){tex, size[0], size[1], size[2], format, type};
}

int gl_texture_check(const gl_tex* const tex) {
	return tex->texture != GL_INIT_DEFAULT_TEX.texture;
}

void gl_texture_update(const gl_tex* const tex, const void* const data, const unsigned int offset[static 3], const unsigned int size[static 3]) {
	switch (tex->type) {
		case gl_textype_2d:
			glBindTexture(GL_TEXTURE_2D, tex->texture);
			glTexSubImage2D(
				GL_TEXTURE_2D, 0,
				offset[0], offset[1],
				size[0], size[1],
				__gl_texformat_lookup[tex->format].format, __gl_texformat_lookup[tex->format].type, data
			);
			glBindTexture(GL_TEXTURE_2D, GL_INIT_DEFAULT_TEX.texture);
			break;
		case gl_textype_2dArray:
			glBindTexture(GL_TEXTURE_2D_ARRAY, tex->texture);
			glTexSubImage3D(
				GL_TEXTURE_2D_ARRAY, 0,
				offset[0], offset[1], offset[2],
				size[0], size[1], size[2],
				__gl_texformat_lookup[tex->format].format, __gl_texformat_lookup[tex->format].type, data
			);
			glBindTexture(GL_TEXTURE_2D_ARRAY, GL_INIT_DEFAULT_TEX.texture);
			break;
		case gl_textype_3d:
			glBindTexture(GL_TEXTURE_3D, tex->texture);
			glTexSubImage3D(
				GL_TEXTURE_3D, 0,
				offset[0], offset[1], offset[2],
				size[0], size[1], size[2],
				__gl_texformat_lookup[tex->format].format, __gl_texformat_lookup[tex->format].type, data
			);
			glBindTexture(GL_TEXTURE_3D, GL_INIT_DEFAULT_TEX.texture);
			break;
	}
}

void gl_texture_bind(const gl_tex* const tex, const gl_param paramId, const unsigned int unit) {
	const GLuint typeLookup[] = {GL_TEXTURE_2D, GL_TEXTURE_3D, GL_TEXTURE_2D_ARRAY};
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(typeLookup[tex->type], tex->texture);
	glUniform1i(paramId, unit);
}

void gl_texture_delete(gl_tex* const tex) {
	glDeleteTextures(1, &tex->texture);
	tex->texture = GL_INIT_DEFAULT_TEX.texture;
}

gl_fbo gl_frameBuffer_create(const unsigned int count, const gl_tex internalBuffer[static 1], const gl_fboattach type[static 1]) {
	for (uint i = 0; i < count; i++) {
		if (internalBuffer[i].type != gl_textype_2d)
			return GL_INIT_DEFAULT_FBO;
	}

	GLuint fbo;
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);

	#ifdef VERBOSE
		__gl_log("Create FBO, id = %u", fbo);
	#endif

	for (uint i = 0; i < count; i++) {
		if (type[i] == gl_fboattach_depth) {
			glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, internalBuffer[i].texture, 0);
			#ifdef VERBOSE
				__gl_log("  - Attach texture to FBO, id = %u, type Depth", internalBuffer[i].texture);
			#endif
		} else if (type[i] == gl_fboattach_stencil) {
			glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, internalBuffer[i].texture, 0);
			#ifdef VERBOSE
				__gl_log("  - Attach texture to FBO, id = %u, type Stencil", internalBuffer[i].texture);
			#endif
		} else if (type[i] == gl_fboattach_depth_stencil) {
			glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, internalBuffer[i].texture, 0);
			#ifdef VERBOSE
				__gl_log("  - Attach texture to FBO, id = %u, type Depth + Stencil", internalBuffer[i].texture);
			#endif
		} else {
			glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + type[i], GL_TEXTURE_2D, internalBuffer[i].texture, 0);
			#ifdef VERBOSE
				__gl_log("  - Attach texture to FBO, id = %u, type color %u", internalBuffer[i].texture, type[i] & 0xFF);
			#endif
		}
	}

	return fbo;
}

int gl_frameBuffer_check(const gl_fbo* const fbo) {
	return *fbo != GL_INIT_DEFAULT_FBO;
}

void gl_frameBuffer_bind(const gl_fbo* const fbo, const int clear) {
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo ? *fbo : 0);

	const int mask[] = {
		0			| 0			| 0			,
		0			| 0			| GL_COLOR_BUFFER_BIT	,
		0			| GL_DEPTH_BUFFER_BIT	| 0			,
		0			| GL_DEPTH_BUFFER_BIT	| GL_COLOR_BUFFER_BIT	,
		GL_STENCIL_BUFFER_BIT	| 0			| 0			,
		GL_STENCIL_BUFFER_BIT	| 0			| GL_COLOR_BUFFER_BIT	,
		GL_STENCIL_BUFFER_BIT	| GL_DEPTH_BUFFER_BIT	| 0			,
		GL_STENCIL_BUFFER_BIT	| GL_DEPTH_BUFFER_BIT	| GL_COLOR_BUFFER_BIT	,
	};
	if (clear) {
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(mask[clear]);
	}
}

void gl_frameBuffer_download(void* const dest, const gl_fbo* const fbo, const gl_texformat format, const unsigned attachment, const unsigned int offset[static 2], const unsigned int size[static 2]) {
	if (format < 0 || format >= gl_texformat_placeholderEnd)
		return;
	
	glBindFramebuffer(GL_READ_FRAMEBUFFER, *fbo);
	glReadBuffer(GL_COLOR_ATTACHMENT0 + attachment);
	glReadPixels(offset[0], offset[1], size[0], size[1], __gl_texformat_lookup[format].format, __gl_texformat_lookup[format].type, dest);
}

void gl_frameBuffer_delete(gl_fbo* const fbo) {
	glDeleteFramebuffers(1, fbo);
	*fbo = GL_INIT_DEFAULT_FBO;
}

gl_pbo gl_pixelBuffer_create(const unsigned int size, const int type, const gl_usage usage) {
	const GLenum usageLookupDraw[] = {GL_STREAM_DRAW, GL_STATIC_DRAW, GL_DYNAMIC_DRAW};
	const GLenum usageLookupRead[] = {GL_STREAM_READ, GL_STATIC_READ, GL_DYNAMIC_READ};
	if (usage < 0 || usage >= gl_usage_placeholderEnd)
		return GL_INIT_DEFAULT_PBO;

	gl_pbo pbo;
	glGenBuffers(1, &pbo);
	if (type) {
		glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
		glBufferData(GL_PIXEL_PACK_BUFFER, size, NULL, usageLookupRead[usage]);
		glBindBuffer(GL_PIXEL_PACK_BUFFER, GL_INIT_DEFAULT_PBO);
	} else {
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
		glBufferData(GL_PIXEL_UNPACK_BUFFER, size, NULL, usageLookupDraw[usage]);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, GL_INIT_DEFAULT_PBO);
	}
	return pbo;
}

int gl_pixelBuffer_check(const gl_pbo* const pbo) {
	return *pbo != GL_INIT_DEFAULT_PBO;
}

void* gl_pixelBuffer_updateStart(const gl_pbo* const pbo, const unsigned int size) {
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, *pbo);
	return glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, size, GL_MAP_WRITE_BIT/* | GL_MAP_INVALIDATE_BUFFER_BIT*/ | GL_MAP_UNSYNCHRONIZED_BIT);
}

void gl_pixelBuffer_updateFinish() {
	glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, GL_INIT_DEFAULT_PBO);
}

void gl_pixelBuffer_updateToTexture(const gl_pbo* const pbo, const gl_tex* const tex) {
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, *pbo);
	switch (tex->type) {
		case gl_textype_2d:
			glBindTexture(GL_TEXTURE_2D, tex->texture);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tex->width, tex->height, __gl_texformat_lookup[tex->format].format, __gl_texformat_lookup[tex->format].type, 0);
			glBindTexture(GL_TEXTURE_2D, GL_INIT_DEFAULT_TEX.texture);
			break;
		case gl_textype_2dArray:
			glBindTexture(GL_TEXTURE_2D_ARRAY, tex->texture);
			glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, tex->width, tex->height, tex->depth, __gl_texformat_lookup[tex->format].format, __gl_texformat_lookup[tex->format].type, 0);
			glBindTexture(GL_TEXTURE_2D_ARRAY, GL_INIT_DEFAULT_TEX.texture);
			break;
		case gl_textype_3d:
			glBindTexture(GL_TEXTURE_3D, tex->texture);
			glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, tex->width, tex->height, tex->depth, __gl_texformat_lookup[tex->format].format, __gl_texformat_lookup[tex->format].type, 0);
			glBindTexture(GL_TEXTURE_3D, GL_INIT_DEFAULT_TEX.texture);
			break;
	}
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, GL_INIT_DEFAULT_PBO);
}

void gl_pixelBuffer_downloadStart(const gl_pbo* pbo, const gl_fbo* const fbo, const gl_texformat format, const unsigned int attachment, const unsigned int offset[static 2], const unsigned int size[static 2]) {
	if (format < 0 || format >= gl_texformat_placeholderEnd)
		return;
		
	glBindFramebuffer(GL_READ_FRAMEBUFFER, *fbo);
	glReadBuffer(GL_COLOR_ATTACHMENT0 + attachment);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, *pbo);
	glReadPixels(offset[0], offset[1], size[0], size[1], __gl_texformat_lookup[format].format, __gl_texformat_lookup[format].type, 0);
}

void* gl_pixelBuffer_downloadFinish(const unsigned int size) {
	void* ptr = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, size, GL_MAP_READ_BIT);
	return ptr;
}

void gl_pixelBuffer_downloadDiscard() {
	glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, GL_INIT_DEFAULT_PBO);
}

void gl_pixelBuffer_delete(gl_pbo* const pbo) {
	glDeleteBuffers(1, pbo);
	*pbo = GL_INIT_DEFAULT_PBO;
}