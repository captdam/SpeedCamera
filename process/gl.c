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

FILE* logStream = NULL; //Log and error log stream
GLFWwindow* window = NULL; //Display window object

void __gl_windowCloseCallback(GLFWwindow* window); //Event callback when window closed by user (X button or kill)
void __gl_glfwErrorCallback(int code, const char* desc); //GLFW error log
void GLAPIENTRY __gl_glErrorCallback(GLenum src, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam); //OpenGL log

gl_synch __gl_synchSet(); //Set a synch point
gl_synch __gl_synchSetPH(); //Set a synch point placeholder
gl_synch_status __gl_synchWait(gl_synch s, uint64_t timeout); //Wait for synch point
gl_synch_status __gl_synchWaitPH(gl_synch s, uint64_t timeout); //Wait for synch point placeholder
void __gl_synchDelete(gl_synch s); //Delete a synch point
void __gl_synchDeletePH(gl_synch s); //Delete a synch point placeholder

void gl_logStream(FILE* stream) {
	logStream = stream;
}

int gl_logWrite(const char* format, ...) {
	va_list args;
	va_start(args, format);
	vfprintf(logStream, format, args);
	va_end(args);
}

int gl_init() {
	#ifdef VERBOSE
		gl_log("Init OpenGL");
	#endif

	/* init GLFW */
	if (!glfwInit()) {
		#ifdef VERBOSE
			gl_elog("\tFail to init GLFW");
		#endif
		gl_destroy();
		return 0;
	}

	/* Start and config OpenGL, init window */
	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
//	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	#ifdef VERBOSE
//		glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
	#endif
	window = glfwCreateWindow(1280, 720, "Viewer", NULL, NULL);
	if (!window){
		#ifdef VERBOSE
			gl_elog("\tFail to open window");
		#endif
		gl_destroy();
		return 0;
	}
	glfwMakeContextCurrent(window);
	glfwSwapInterval(0);

	/* Init GLEW */
	GLenum glewInitError = glewInit();
	if (glewInitError != GLEW_OK) {
		#ifdef VERBOSE
			gl_elog("\tFail init GLEW: %s", glewGetErrorString(glewInitError));
		#endif
		gl_destroy();
		return 0;
	}

	/* Driver and hardware info */
	#ifdef VERBOSE
		gl_log("OpenGL driver and hardware info:");
		int textureSize, textureSizeArray, textureSize3d, textureImageUnit, textureImageUnitVertex;
		glGetIntegerv(GL_MAX_TEXTURE_SIZE, &textureSize);
		glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &textureSizeArray);
		glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &textureSize3d);
		glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &textureImageUnit);
		glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &textureImageUnitVertex);
		gl_log("\t- Vendor:                   %s", glGetString(GL_VENDOR));
		gl_log("\t- Renderer:                 %s", glGetString(GL_RENDERER));
		gl_log("\t- Version:                  %s", glGetString(GL_VERSION));
		gl_log("\t- Shader language version:  %s", glGetString(GL_SHADING_LANGUAGE_VERSION));
		gl_log("\t- Max texture size:         %d", textureSize);
		gl_log("\t- Max texture layers:       %d", textureSizeArray);
		gl_log("\t- Max 3D texture size:      %d", textureSize3d);
		gl_log("\t- Max texture Units:        %d", textureImageUnit);
		gl_log("\t- Max Vertex texture units: %d", textureImageUnitVertex);
	#endif

	/* Setup functions and their alternative placeholder */
	if (glFenceSync && glClientWaitSync && glDeleteSync) {
		gl_synchSet = &__gl_synchSet;
		gl_synchWait = &__gl_synchWait;
		gl_synchDelete = &__gl_synchDelete;
	} else {
		gl_synchSet = &__gl_synchSetPH;
		gl_synchWait = &__gl_synchWaitPH;
		gl_synchDelete = &__gl_synchDeletePH;
	}

	/* Event control - callback */
	glfwSetWindowCloseCallback(window, __gl_windowCloseCallback);
	glfwSetErrorCallback(__gl_glfwErrorCallback);
	glDebugMessageCallback(__gl_glErrorCallback, NULL);

	/* OpenGL config */
//	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glEnable(GL_PROGRAM_POINT_SIZE); glPointSize(10.0f);
	glfwSetCursor(window, glfwCreateStandardCursor(GLFW_CROSSHAIR_CURSOR));

	return 1;
}

void gl_drawStart(double cursorPos[static 2], int windowSize[static 2], int framebufferSize[static 2]) {
	glfwPollEvents();
	glfwGetCursorPos(window, cursorPos, cursorPos+1);
	glfwGetWindowSize(window, windowSize, windowSize+1);
	glfwGetFramebufferSize(window, framebufferSize, framebufferSize+1);
}


void gl_setViewport(const unsigned int offset[static 2], const unsigned int size[static 2]) {
	glViewport(offset[0], offset[1], size[0], size[1]);
}

void gl_drawEnd(const char* title) {
	if (title)
		glfwSetWindowTitle(window, title);
	
	glfwSwapBuffers(window);
}

int gl_close(int close) {
	if (!close) {
		glfwSetWindowShouldClose(window, GLFW_FALSE);
		close = GLFW_FALSE;
	} else if (close > 0) {
		glfwSetWindowShouldClose(window, GLFW_TRUE);
		close = GLFW_TRUE;
	} else {
		close = glfwWindowShouldClose(window);
	}
	return close;
}

void gl_destroy() {
	#ifdef VERBOSE
		gl_log("Destroy OpenGL");
	#endif
	gl_close(1);
	glfwTerminate();
}

void gl_lineMode(unsigned int weight) {
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

gl_synch __gl_synchSet() {
	gl_synch s = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	return s;
}
gl_synch __gl_synchSetPH() {
	return NULL;
}
gl_synch_status __gl_synchWait(gl_synch s, uint64_t timeout) {
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
gl_synch_status __gl_synchWaitPH(gl_synch s, uint64_t timeout) {
	gl_fsync();
	return gl_synch_ok;
}
void __gl_synchDelete(gl_synch s) {
	glDeleteSync(s);
}
void __gl_synchDeletePH(gl_synch s) {
	return;
}

void __gl_windowCloseCallback(GLFWwindow* window) {
	gl_log("Window close event fired");
	glfwSetWindowShouldClose(window, GLFW_TRUE);
}
void __gl_glfwErrorCallback(int code, const char* desc) {
	gl_elog("GLFW error %d: %s", code, desc);
}
void GLAPIENTRY __gl_glErrorCallback(GLenum src, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
	if (type == GL_DEBUG_TYPE_ERROR)
		gl_elog("GL Driver: type %x, severity %x, message: %s", type, severity, message);
	else
		gl_log("GL Driver: type %x, severity %x, message: %s", type, severity, message);
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
char* __gl_loadFileToMemory(const char* filename, long int* length);

/** Free the buffer allocated by file reading.
 * @param buffer Buffer returned by __gl_loadFileToMemory()
 */
void __gl_unloadFileFromMemory(char* buffer);

void gl_program_setCommonHeader(const char* header) {
	shaderCommonHeader = (char*)header;
}

gl_program gl_program_create(const gl_programSrc* srcs, gl_programArg* args) {
	const int shaderTypeLookup[] = {GL_VERTEX_SHADER, GL_FRAGMENT_SHADER, GL_GEOMETRY_SHADER}; //Associated with enum GL_ProgramSourceCodeType
	const char* shaderTypeDescription[] = {"vertex", "fragment", "geometry (optional)"};

	#ifdef VERBOSE
		gl_log("Init shader program");
	#endif

	uint32_t shaderTypePresent = 0x00000000;

	/* Check function param */ {
		for (gl_programSrc* s = (gl_programSrc*)srcs; s->str; s++) {
			if (s->type >= gl_programSrcType_placeholderEnd || s->type < 0) {
				gl_elog("Bad shader code type");
				return GL_INIT_DEFAULT_PROGRAM;
			}
			if (s->loc >= gl_programSrcLoc_placeholderEnd || s->loc < 0) {
				gl_elog("Bad shader code source");
				return GL_INIT_DEFAULT_PROGRAM;
			}
			shaderTypePresent |= 1 << s->type;
		}
		for (gl_programArg* a = args; a->name; a++) {
			if (a->type >= gl_programArgType_placeholderEnd || a->type < 0) {
				gl_elog("Bad shader program argument type");
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
				gl_elog("Fail to write code for %s shader buffer, section header", shaderTypeDescription[type]);
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
				gl_log("  - Load section %u of %s shader, from memory @ %p", shaderBuffer[type].count, shaderTypeDescription[type], str);
			else
				gl_log("  - Load section %u of %s shader, from file: %s", shaderBuffer[type].count, shaderTypeDescription[type], str);
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
			gl_elog("Fail to write header for %s shader buffer, section %u", shaderTypeDescription[type], shaderBuffer[type].count);
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
					gl_elog("Fail to load shader from file %s: Cannot open file", str);
				else if (len < 0)
					gl_elog("Fail to load shader from file %s: Cannot get file length", str);
				else
					gl_elog("Fail to load shader from file %s: Cannot create buffer", str);
				destroy_all_shader_buffer();
				return GL_INIT_DEFAULT_PROGRAM;
			}
		}

		/* Write shader code content to buffer */
		if (!( shaderBuffer[type].buffer = realloc(shaderBuffer[type].buffer, shaderBuffer[type].size + len + 1) )) {
			gl_elog("Fail to write code for %s shader buffer, section %u", shaderTypeDescription[type], shaderBuffer[type].count);
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
			gl_elog("Cannot create shader program");
			destroy_all_shader_buffer();
			return GL_INIT_DEFAULT_PROGRAM;
		}
		#ifdef VERBOSE
			gl_log("  - Create shader program, id = %u", program);
		#endif
		for (unsigned int type = 0; type < gl_programSrcType_placeholderEnd; type++) {
			if (shaderBuffer[type].buffer) { //Compile only if tis shader type is presented
				if (!( shader = glCreateShader(shaderTypeLookup[type]) )) {
					gl_elog("Cannot create %s shader", shaderTypeDescription[type]);
					glDeleteProgram(program);
					destroy_all_shader_buffer();
					return GL_INIT_DEFAULT_PROGRAM;
				}
				#ifdef VERBOSE
					gl_log("  - Create and compile shader, id = %u: %s shader", shader, shaderTypeDescription[type]);
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
					gl_elog("Error in %s shader:\n%s", shaderTypeDescription[type], msg);
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
			gl_elog("Shader program link fail:\n%s", msg);
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
				gl_elog("Shader program argument bind fail: Argument %s is not in shader code\n", a->name);
				glDeleteProgram(program);
				return GL_INIT_DEFAULT_PROGRAM;
			}
			#ifdef VERBOSE
				gl_log("  - Shader program argument \"%s\" bind to %d", a->name, a->id);
			#endif
		}
	}

	#ifdef VERBOSE
		GLuint binSize;
		glGetProgramiv(program, GL_PROGRAM_BINARY_LENGTH, &binSize);
		gl_log("  - Shader program (%u) create success, size %u", program, binSize);
	#endif
	return program;
}

int gl_program_check(gl_program* program) {
	return *program != GL_INIT_DEFAULT_PROGRAM;
}

void gl_program_use(gl_program* program) {
	glUseProgram(*program);
}

void gl_program_setParam(const gl_param paramId, const unsigned int length, const gl_datatype type, const void* data) {
	if (length - 1 > 3) {
		gl_elog("Param set fail: GL supports vector size 1 to 4 only");
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
		gl_elog("Param set fail: GL supports date type int, uint and float only");
}

void gl_program_delete(gl_program* program) {
	glDeleteProgram(*program);
	*program = GL_INIT_DEFAULT_PROGRAM;
}

gl_ubo gl_uniformBuffer_create(unsigned int bindingPoint, unsigned int size, gl_usage usage) {
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

int gl_uniformBuffer_check(gl_ubo* ubo) {
	return *ubo != GL_INIT_DEFAULT_UBO;
}

void gl_uniformBuffer_bindShader(unsigned int bindingPoint, gl_program* program, gl_param paramId) {
	glUniformBlockBinding(*program, paramId, bindingPoint);
}

void gl_uniformBuffer_update(gl_ubo* ubo, unsigned int start, unsigned int len, void* data) {
	glBindBuffer(GL_UNIFORM_BUFFER, *ubo);
	glBufferSubData(GL_UNIFORM_BUFFER, start, len, data);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void gl_unifromBuffer_delete(gl_ubo* ubo) {
	glDeleteBuffers(1, ubo);
	*ubo = GL_INIT_DEFAULT_UBO;
}
char* __gl_loadFileToMemory(const char* filename, long int* length) {
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
void __gl_unloadFileFromMemory(char* buffer) {
	free(buffer);
}

/* == Mesh (vertices) ======================================================================= */

gl_mesh gl_mesh_create(const unsigned int count[static 3], gl_index_t* elementsSize, gl_vertex_t* vertices, gl_index_t* indices, gl_meshmode mode, gl_usage usage) {
	const GLenum usageLookup[] = {GL_STREAM_DRAW, GL_STATIC_DRAW, GL_DYNAMIC_DRAW};
	if (usage < 0 || usage >= gl_usage_placeholderEnd)
		return GL_INIT_DEFAULT_MESH;
	
	const unsigned int vertexCount = count[0], verticesCount = count[1], indicesCount = count[2];
	
	gl_mesh mesh = GL_INIT_DEFAULT_MESH;
	mesh.drawSize = indices ? indicesCount :verticesCount;
	switch(mode) {
		case gl_meshmode_triangles:
			mesh.mode = GL_TRIANGLES;
			break;
		case gl_meshmode_points:
			mesh.mode = GL_POINTS;
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

	glGenVertexArrays(1, &mesh.vao);
	glGenBuffers(1, &mesh.vbo);
	if (indices)
		glGenBuffers(1, &mesh.ebo);
	
	glBindVertexArray(mesh.vao);
	glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
	glBufferData(GL_ARRAY_BUFFER, verticesCount * vertexCount * sizeof(gl_vertex_t), vertices, usageLookup[usage]);
	if (indices) {
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indicesCount * sizeof(gl_index_t), indices, usageLookup[usage]);
	}
	
	GLuint elementIndex = 0, attrIndex = 0;
	while (elementIndex < vertexCount) {
		glVertexAttribPointer(attrIndex, *elementsSize, GL_FLOAT, GL_FALSE, vertexCount * sizeof(float), (GLvoid*)(elementIndex * sizeof(float)));
		glEnableVertexAttribArray(attrIndex);
		elementIndex += *(elementsSize++);
		attrIndex++;
	}
	
	glBindBuffer(GL_ARRAY_BUFFER, GL_INIT_DEFAULT_MESH.vbo);
	glBindVertexArray(GL_INIT_DEFAULT_MESH.vao);
	return mesh;
}

int gl_mesh_check(gl_mesh* mesh) {
	return mesh->vao != GL_INIT_DEFAULT_MESH.vao && mesh->vbo != GL_INIT_DEFAULT_MESH.vbo;
}

void gl_mesh_update(gl_mesh* mesh, gl_vertex_t* vertices, gl_index_t* indices, const unsigned int offCnt[static 4]) {
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

void gl_mesh_draw(gl_mesh* mesh) {
	glBindVertexArray(mesh->vao);
	if (mesh->ebo)
		glDrawElements(mesh->mode, mesh->drawSize, GL_UNSIGNED_INT, 0);
	else
		glDrawArrays(mesh->mode, 0, mesh->drawSize);
	glBindVertexArray(GL_INIT_DEFAULT_MESH.vao);
}

void gl_mesh_delete(gl_mesh* mesh) {
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
	{gl_texformat_RGBA32UI,		GL_RGBA32UI,	GL_RGBA_INTEGER,	GL_UNSIGNED_INT		}
};

gl_tex gl_texture_create(gl_texformat format, gl_textype type, const unsigned int size[static 3]) {
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
			glBindTexture(GL_TEXTURE_2D, tex);
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

int gl_texture_check(gl_tex* tex) {
	return tex->texture != GL_INIT_DEFAULT_TEX.texture;
}

void gl_texture_update(gl_tex* tex, void* data, const unsigned int offset[static 3], const unsigned int size[static 3]) {
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

void gl_texture_bind(gl_tex* tex, gl_param paramId, unsigned int unit) {
	const GLuint typeLookup[] = {GL_TEXTURE_2D, GL_TEXTURE_3D, GL_TEXTURE_2D_ARRAY};
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(typeLookup[tex->type], tex->texture);
	glUniform1i(paramId, unit);
}

void gl_texture_delete(gl_tex* tex) {
	glDeleteTextures(1, &tex->texture);
	tex->texture = GL_INIT_DEFAULT_TEX.texture;
}

gl_pbo gl_pixelBuffer_create(unsigned int size, gl_usage usage) {
	const GLenum usageLookup[] = {GL_STREAM_DRAW, GL_STATIC_DRAW, GL_DYNAMIC_DRAW};
	if (usage < 0 || usage >= gl_usage_placeholderEnd)
		return GL_INIT_DEFAULT_PBO;

	gl_pbo pbo;
	glGenBuffers(1, &pbo);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
	glBufferData(GL_PIXEL_UNPACK_BUFFER, size, NULL, usageLookup[usage]);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, GL_INIT_DEFAULT_PBO);
	return pbo;
}

int gl_pixelBuffer_check(gl_pbo* pbo) {
	return *pbo != GL_INIT_DEFAULT_PBO;
}

void* gl_pixelBuffer_updateStart(gl_pbo* pbo, unsigned int size) {
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, *pbo);
	return glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, size, GL_MAP_WRITE_BIT/* | GL_MAP_INVALIDATE_BUFFER_BIT*/ | GL_MAP_UNSYNCHRONIZED_BIT);
}

void gl_pixelBuffer_updateFinish() {
	glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, GL_INIT_DEFAULT_PBO);
}

void gl_pixelBuffer_updateToTexture(gl_pbo* pbo, gl_tex* tex) {
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

void gl_pixelBuffer_delete(gl_pbo* pbo) {
	glDeleteBuffers(1, pbo);
	*pbo = GL_INIT_DEFAULT_PBO;
}

gl_fbo gl_frameBuffer_create(const gl_tex internalBuffer[static 1], unsigned int count) {
	for (unsigned i = 0; i < count; i++) {
		if (internalBuffer[i].type != gl_textype_2d)
			return GL_INIT_DEFAULT_FBO;
	}

	GLuint fbo;
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);

	for (unsigned int i = 0; i < count; i++)
		glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, internalBuffer[i].texture, 0);

	return fbo;
}

int gl_frameBuffer_check(gl_fbo* fbo) {
	return *fbo != GL_INIT_DEFAULT_FBO;
}

void gl_frameBuffer_bind(gl_fbo* fbo, int clear) {
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo ? *fbo : 0);

	if (clear) {
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
	}
}

void gl_frameBuffer_download(gl_fbo* fbo, void* dest, gl_texformat format, const unsigned int attachment, const unsigned int offset[static 2], const unsigned int size[static 2]) {
	if (format < 0 || format >= gl_texformat_placeholderEnd)
		return;
	
	glBindFramebuffer(GL_READ_FRAMEBUFFER, *fbo);
	glReadBuffer(GL_COLOR_ATTACHMENT0 + attachment);
	
	glReadPixels(offset[0], offset[1], size[0], size[1], __gl_texformat_lookup[format].format, __gl_texformat_lookup[format].type, dest);
}

void gl_frameBuffer_delete(gl_fbo* fbo) {
	glDeleteFramebuffers(1, fbo);
	*fbo = GL_INIT_DEFAULT_FBO;
}