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

#define __gl_elog(format, ...) (fprintf(stderr, "[GL] Err:\t"format"\n" __VA_OPT__(,) __VA_ARGS__))
#ifdef VERBOSE
#define __gl_log(format, ...) (fprintf(stderr, "[GL] Log:\t"format"\n" __VA_OPT__(,) __VA_ARGS__))
#else
#define __gl_log(format, ...)
#endif

int gl_init(gl_config config) {
	__gl_log("Init OpenGL");

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

	glfwSwapInterval(config.vsynch);

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

	/* Event control - callback */
	glfwSetWindowCloseCallback(window, __gl_windowCloseCallback);
	glfwSetErrorCallback(__gl_glfwErrorCallback);
	glDebugMessageCallback(__gl_glErrorCallback, NULL);

	/* OpenGL config */
//	glEnable(GL_PROGRAM_POINT_SIZE); glPointSize(10.0f);
//	glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS);
//	glEnable(GL_CULL_FACE); glCullFace(GL_BACK); glFrontFace(GL_CCW);
//	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
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
		__gl_log("Request to keep window");
		glfwSetWindowShouldClose(window, 0);
		return 0;
	} else if (close > 0) {
		__gl_log("Request to close window");
		glfwSetWindowShouldClose(window, 1);
		return 1;
	} else {
		return glfwWindowShouldClose(window);
	}
}

void gl_destroy() {
	__gl_log("Destroy OpenGL");
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

void gl_program_setCommonHeader(const char* const header) {
	shaderCommonHeader = (char*)header;
}

gl_program gl_program_load(char* src, gl_programArg* const args) {
	//Create a pointer to src code
	char* code = NULL;
	if (src[0] == 'f') { //From file (1)
		__gl_log("Init shader program from file: %s", src+1);
		FILE* fp = fopen(src+1, "r");
		if (!fp) {
			__gl_elog("Cannot open shader source code file: %s", src+1);
			return GL_INIT_DEFAULT_PROGRAM;
		}
		fseek(fp, 0, SEEK_END);
		long int len = ftell(fp);
		if (len < 0) {
			fclose(fp);
			__gl_elog("Cannot get shader source code file length: %s", src+1);
			return GL_INIT_DEFAULT_PROGRAM;
		}
		__gl_log("  - File size: %ld", len);
		code = alloca(len + 1); //Shader code is normally small
		rewind(fp);
		!fread(code, 1, len, fp);
		code[len] = '\0';
		fclose(fp);
	} else { //From memory (0)
		__gl_log("Init shader program from memory @ %p", src);
		code = src + 1;
	}

	const unsigned int shaderTypes[] = {GL_VERTEX_SHADER, GL_FRAGMENT_SHADER, GL_GEOMETRY_SHADER};
	const char* shaderTokens[] = {"@VS\n", "@FS\n", "@GS\n"};
	const unsigned int shaderCnt = sizeof(shaderTypes) / sizeof(shaderTypes[0]);
	struct {
		GLuint name; //GL shader name
		unsigned int line; //Shader line number in file
		char* ptr; //Pointer to start of each src code
	} shaders[shaderCnt];

	GLint status;
	GLuint msgLength, program = glCreateProgram();
	if (!program) {
		__gl_elog("Cannot create new empty shader program");
		return GL_INIT_DEFAULT_PROGRAM;
	}

	for (int i = 0; i < shaderCnt; i++) { //Pass 1: Get ptr to specific shader and line number
		shaders[i].ptr = strstr(code, shaderTokens[i]);
		if (shaders[i].ptr) {
			shaders[i].line = 1;
			for (char* x = code; x < shaders[i].ptr; x++) { if (*x == '\n') shaders[i].line++; }
			__gl_log("  - Get shader %.2s, line %u, offset %lu @ %p", shaderTokens[i]+1, shaders[i].line, shaders[i].ptr - code, shaders[i].ptr);
		}
	}
	for (int i = 0; i < shaderCnt; i++) { //Pass 2: Replace toekn to '\0'
		if (shaders[i].ptr)
			memset(shaders[i].ptr, 0, strlen(shaderTokens[0]));
	}
	for (int i = 0; i < shaderCnt; i++) { //Pass 3: Create shader
		if (!shaders[i].ptr)
			continue;
		if (!( shaders[i].name = glCreateShader(shaderTypes[i]) )) {
			__gl_elog("Cannot create new empty %.2s shader", shaderTokens[i]+1);
			glDeleteProgram(program);
			return GL_INIT_DEFAULT_PROGRAM;
		}
		char numberLine[32];
		snprintf(numberLine, 32, "#line %u\n", shaders[i].line);
		const GLchar* x[3] = {shaderCommonHeader, numberLine, shaders[i].ptr + strlen(shaderTokens[0])};
		glShaderSource(shaders[i].name, 3, x, NULL);
		glCompileShader(shaders[i].name);
		glGetShaderiv(shaders[i].name, GL_COMPILE_STATUS, &status);
		if (!status) {
			glGetShaderiv(shaders[i].name, GL_INFO_LOG_LENGTH, &msgLength);
			char msg[msgLength];
			glGetShaderInfoLog(shaders[i].name, msgLength, NULL, msg);
			if (src[0] == 'f')
				__gl_elog("Error in %.2s shader from file \"%s\":\n%s", shaderTokens[i]+1, src+1, msg);
			else
				__gl_elog("Error in %.2s shader @ %p:\n%s", shaderTokens[i]+1, src, msg);
			glDeleteShader(shaders[i].name); //Delete current shader
			glDeleteProgram(program); //Delete program and all previous shaders, previous shaders have attached and flag for delete
			return GL_INIT_DEFAULT_PROGRAM;
		}
		glAttachShader(program, shaders[i].name); //Attach current shader, so we can flag them delete for automatical delete
		glDeleteShader(shaders[i].name); //Shader compiled and attached, flag the shader for delete when the program is delete
	}

	glLinkProgram(program);
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (!status) {
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &msgLength);
		char msg[msgLength];
		glGetProgramInfoLog(program, msgLength, NULL, msg);
		if (src[0] == 'f')
			__gl_elog("Shader program from file \"%s\" link fail:\n%s", src+1, msg);
		else
			__gl_elog("Shader program @ %p link fail:\n%s", src, msg);
		glDeleteProgram(program);
		return GL_INIT_DEFAULT_PROGRAM;
	}

	/* Bind arguments */
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
			if (src[0] == 'f')
				__gl_elog("Shader program argument bind fail: Argument \"%s\" is not in shader code from file \"%s\"", a->name, src+1);
			else
				__gl_elog("Shader program argument bind fail: Argument \"%s\" is not in shader code @ %p", a->name, src);
			glDeleteProgram(program);
			return GL_INIT_DEFAULT_PROGRAM;
		}
		__gl_log("  - Shader program argument \"%s\" bind to %d", a->name, a->id);
	}

	GLuint binSize;
	glGetProgramiv(program, GL_PROGRAM_BINARY_LENGTH, &binSize);
	__gl_log("  - Shader program (%u) create success, size %u", program, binSize);
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

/* == Mesh (vertices) ======================================================================= */

gl_mesh gl_mesh_create(
	const unsigned int vCnt, const unsigned int eCnt, const unsigned int iCnt, const gl_meshmode mode,
	const gl_index_t* const vSize, const gl_index_t* const iSize,
	const gl_vertex_t* const vertices, const gl_index_t* const indices, const gl_mesh* const shareInstance
) {
	__gl_log("Create mesh, %s%sM%u", eCnt ? "Indices " : "", iCnt ? "Instances " : "", mode);
	gl_mesh mesh = {
		.vao = 0, .vbo = 0, .ebo = 0, .ibo = 0,
		.drawSize = eCnt ? eCnt : vCnt, //Set to face count if applied, otherwise use vertice size
		.mode = mode
	};
	glGenVertexArrays(1, &mesh.vao);
	glBindVertexArray(mesh.vao);
	__gl_log("  - VAO Id = %u", mesh.vao);

	gl_index_t aIdx = 0, vboStride = 0, iboStride = 0; //Get attribute index in shader program, get vertex stride (in unit of number, not byte) for vbo and ibo
	for (const gl_index_t* x = vSize; *x; x++)
		vboStride += *x;
	if (iCnt) {
		for (const gl_index_t* x = iSize; *x; x++)
			iboStride += *x;
	}

	/* Set vertices */
	glGenBuffers(1, &mesh.vbo);
	glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
	__gl_log("  - VBO Id = %u", mesh.vbo);
	glBufferData(GL_ARRAY_BUFFER, vCnt * vboStride * sizeof(gl_vertex_t), vertices, GL_STATIC_DRAW); //Mesh (model) is infrequently updated
	for (unsigned int i = 0, offset = 0; vSize[i]; i++) {
		glEnableVertexAttribArray(aIdx);
		glVertexAttribPointer(aIdx, vSize[i], GL_FLOAT, GL_FALSE, vboStride * sizeof(gl_vertex_t), (GLvoid*)(offset * sizeof(gl_vertex_t)));
		offset += vSize[i];
		aIdx++;
	}
	__gl_log("  - Vertices attributes bind");
	
	/* Set indices, if applied */
	if (eCnt) {
		glGenBuffers(1, &mesh.ebo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
		__gl_log("  - EBO Id = %u", mesh.ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, eCnt * sizeof(gl_index_t), indices, GL_STATIC_DRAW);
	}

	/* Set instance, if applied */
	if (iCnt) {
		if (shareInstance) {
			mesh.ibo = shareInstance->ibo;
			glBindBuffer(GL_ARRAY_BUFFER, mesh.ibo);
			__gl_log("  - IBO Id = %u (shared with mesh VAO %u)", mesh.ibo, shareInstance->vao);
		} else {
			glGenBuffers(1, &mesh.ibo);
			glBindBuffer(GL_ARRAY_BUFFER, mesh.ibo);
			__gl_log("  - IBO Id = %u", mesh.ibo);
			glBufferData(GL_ARRAY_BUFFER, iCnt * iboStride * sizeof(gl_vertex_t), NULL, GL_STREAM_DRAW); //Instance of mesh is frequently updates
		}
		for (unsigned int i = 0, offset = 0; iSize[i]; i++) {
			glEnableVertexAttribArray(aIdx);
			glVertexAttribPointer(aIdx, iSize[i], GL_FLOAT, GL_FALSE, iboStride * sizeof(gl_vertex_t), (GLvoid*)(offset * sizeof(gl_vertex_t)));
			glVertexAttribDivisor(aIdx, 1);
			offset += iSize[i];
			aIdx++;
		}
		__gl_log("  - Instances attributes bind");
	}
	
	return mesh;
}

int gl_mesh_check(const gl_mesh* const mesh) {
	return mesh->vao && mesh->vbo;
}

void gl_mesh_updateVertices(const gl_mesh* const mesh, const gl_vertex_t* const vertices, const unsigned int start, const unsigned int len) {
	glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
	glBufferSubData(GL_ARRAY_BUFFER, start * sizeof(gl_vertex_t), len * sizeof(gl_vertex_t), vertices);
}

void gl_mesh_updateIndices(const gl_mesh* const mesh, const gl_index_t* const indices, const unsigned int start, const unsigned int len) {
	glBindVertexArray(mesh->vao);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ebo);
	glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, start * sizeof(gl_index_t), len * sizeof(gl_index_t), indices);
}

void gl_mesh_updateInstances(const gl_mesh* const mesh, const gl_vertex_t* const instances, const unsigned int start, const unsigned int len) {
	glBindBuffer(GL_ARRAY_BUFFER, mesh->ibo);
	glBufferSubData(GL_ARRAY_BUFFER, start * sizeof(gl_vertex_t), len * sizeof(gl_vertex_t), instances);
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
	glBindVertexArray(mesh->vao); //Remove reference to EBO in VAO
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
	glBindVertexArray(0);

	glDeleteVertexArrays(1, &mesh->vao);
	glDeleteBuffers(1, &mesh->vbo);
	glDeleteBuffers(1, &mesh->ebo);
	glDeleteBuffers(1, &mesh->ibo);
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
	{gl_texformat_s8,	GL_STENCIL_INDEX8,	GL_STENCIL_INDEX,	GL_UNSIGNED_BYTE	}
};

gl_tex gl_texture_create(const gl_texformat format, const gl_textype type, const gl_tex_dimFilter minFilter, const gl_tex_dimFilter magFilter, const gl_tex_dim dim[static 3]) {
	GLuint tex;

	unsigned int mipmap = (unsigned int)minFilter & (unsigned int)0x0100 ? 1 : 0; //mipmap=0x2700-0x2703, non-mipmap=0x2600-0x2601, so check bit 8

	switch (type) {
		case gl_textype_2d:
			glGenTextures(1, &tex);
			glBindTexture(GL_TEXTURE_2D, tex);
			glTexImage2D(
				GL_TEXTURE_2D, 0, __gl_texformat_lookup[format].internalFormat,
				dim[0].size, dim[1].size,
				0, __gl_texformat_lookup[format].format, __gl_texformat_lookup[format].type, NULL
			);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter); //GL_LINEAR
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, dim[0].wrapping);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, dim[1].wrapping);
			break;
		case gl_textype_2dArray:
			glGenTextures(1, &tex);
			glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
			glTexImage3D(
				GL_TEXTURE_2D_ARRAY, 0, __gl_texformat_lookup[format].internalFormat,
				dim[0].size, dim[1].size, dim[2].size,
				0, __gl_texformat_lookup[format].format, __gl_texformat_lookup[format].type, NULL
			);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, minFilter);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, magFilter);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, dim[0].wrapping);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, dim[1].wrapping);
			break;
		case gl_textype_3d:
			glGenTextures(1, &tex);
			glBindTexture(GL_TEXTURE_3D, tex);
			glTexImage3D(
				GL_TEXTURE_3D, 0, __gl_texformat_lookup[format].internalFormat,
				dim[0].size, dim[1].size, dim[2].size,
				0, __gl_texformat_lookup[format].format, __gl_texformat_lookup[format].type, NULL
			);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, minFilter);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, magFilter);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, dim[0].wrapping);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, dim[1].wrapping);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, dim[2].wrapping);
			break;
		case gl_textype_renderBuffer:
			glGenRenderbuffers(1, &tex);
			glBindRenderbuffer(GL_RENDERBUFFER, tex);
			glRenderbufferStorage(GL_RENDERBUFFER, __gl_texformat_lookup[format].internalFormat, dim[0].size, dim[1].size);
			break;
	}

	return (gl_tex){tex, dim[0].size, dim[1].size, dim[2].size, format, type, mipmap};
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
			if (tex->mipmap)
				glGenerateMipmap(GL_TEXTURE_2D);
			break;
		case gl_textype_2dArray:
			glBindTexture(GL_TEXTURE_2D_ARRAY, tex->texture);
			glTexSubImage3D(
				GL_TEXTURE_2D_ARRAY, 0,
				offset[0], offset[1], offset[2],
				size[0], size[1], size[2],
				__gl_texformat_lookup[tex->format].format, __gl_texformat_lookup[tex->format].type, data
			);
			if (tex->mipmap)
				glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
			break;
		case gl_textype_3d:
			glBindTexture(GL_TEXTURE_3D, tex->texture);
			glTexSubImage3D(
				GL_TEXTURE_3D, 0,
				offset[0], offset[1], offset[2],
				size[0], size[1], size[2],
				__gl_texformat_lookup[tex->format].format, __gl_texformat_lookup[tex->format].type, data
			);
			if (tex->mipmap)
				glGenerateMipmap(GL_TEXTURE_3D);
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
	if (tex->type == gl_textype_renderBuffer)
		glDeleteRenderbuffers(1,&tex->texture );
	else
		glDeleteTextures(1, &tex->texture);
	tex->texture = GL_INIT_DEFAULT_TEX.texture;
}

gl_fbo gl_frameBuffer_create(const unsigned int count, const gl_tex internalBuffer[static 1], const gl_fboattach type[static 1]) {
	GLuint fbo;
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);

	__gl_log("Create FBO, id = %u", fbo);
	unsigned int targetCnt = 0;
	for (uint i = 0; i < count; i++) {
		if (type[i] == gl_fboattach_depth) {
			if (internalBuffer[i].type == gl_textype_renderBuffer)
				glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, internalBuffer[i].texture);
			else
				glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, internalBuffer[i].texture, 0);
			__gl_log("  - Attach texture / renderbuffer to FBO, id = %u, type Depth", internalBuffer[i].texture);
		} else if (type[i] == gl_fboattach_stencil) {
			if (internalBuffer[i].type == gl_textype_renderBuffer)
				glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, internalBuffer[i].texture);
			else
				glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, internalBuffer[i].texture, 0);
			__gl_log("  - Attach texture / renderbuffer to FBO, id = %u, type Stencil", internalBuffer[i].texture);
		} else if (type[i] == gl_fboattach_depth_stencil) {
			if (internalBuffer[i].type == gl_textype_renderBuffer)
				glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, internalBuffer[i].texture);
			else
				glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, internalBuffer[i].texture, 0);
			__gl_log("  - Attach texture / renderbuffer to FBO, id = %u, type Depth + Stencil", internalBuffer[i].texture);
		} else {
			if (internalBuffer[i].type == gl_textype_renderBuffer)
				glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, internalBuffer[i].texture);
			else
				glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + type[i], GL_TEXTURE_2D, internalBuffer[i].texture, 0);
			__gl_log("  - Attach texture / renderbuffer to FBO, id = %u, type color %u", internalBuffer[i].texture, type[i] & 0xFF);
			targetCnt++;
		}
	}

	/*__gl_log("  - %u color render targets bound", targetCnt);
	GLuint target[targetCnt];
	for (uint i = 0; i < count; i++) {
		if (type[i] >= 0) {
			target[i] =  GL_COLOR_ATTACHMENT0 + type[i];
		}
	}
	glDrawBuffers(targetCnt, target);*/

	return fbo;
}

int gl_frameBuffer_check(const gl_fbo* const fbo) {
	return *fbo != GL_INIT_DEFAULT_FBO;
}

void gl_frameBuffer_bind(const gl_fbo* const fbo, const int clear) {
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo ? *fbo : 0);
	if (clear) {
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(clear);
	}
}

void gl_frameBuffer_download(const gl_fbo* const fbo, void* const dest, const gl_texformat format, const unsigned int attachment, const unsigned int offset[static 2], const unsigned int size[static 2]) {
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