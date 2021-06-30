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

#include "gl.h"

struct GL_ClassDataStructure {
	GLFWwindow* window; //Obj
	struct timespec renderLoopStartTime;
	gl_mesh windowDefaultBufferMesh; //Obj
	gl_obj windowDefaultBufferProgram; //Obj
	size2d_t windowSize;
};

/* == Window management and driver init == [Object] ========================================= */

/* Event callback when window closed by user (X button or kill) */
void gl_windowCloseCallback(GLFWwindow* window);

/* GLFW error log. Non-critical error, program should not be killed in this case, just print them in stderr */
void gl_glfwErrorCallback(int code, const char* desc);

/** OpenGL error log.  Non-critical error, program should not be killed in this case, just print them in stderr */
void GLAPIENTRY gl_glErrorCallback(GLenum src, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam);

GL gl_init(size2d_t frameSize, unsigned int windowRatio) {
	#ifdef VERBOSE
		fputs("Init GL class object\n", stdout);
		fflush(stdout);
	#endif

	/* Object init */
	GL this = malloc(sizeof(struct GL_ClassDataStructure));
	if (!this) {
		#ifdef VERBOSE
				fputs("\tFail to create gl class object data structure\n", stderr);
		#endif

		return NULL;
	}

	/* Default values of all objects (null-equivalent, can be passed to destructor safely without init) */
	this->window = NULL;
	this->windowDefaultBufferMesh = (gl_mesh){.drawSize=0, .vao=0, .vbo=0, .ebo=0};
	this->windowDefaultBufferProgram = 0;
	/* and default values to non-object variables */
	this->renderLoopStartTime = (struct timespec){.tv_nsec=0, .tv_sec=0};
	this->windowSize = (size2d_t){.width = frameSize.width / windowRatio, .height = frameSize.height / windowRatio};

	/* init GLFW */
	if (!glfwInit()) {
		#ifdef VERBOSE
				fputs("\tFail to init GLFW\n", stderr);
		#endif

		gl_destroy(this);
		return NULL;
	}

	/* Start and config OpenGL, init window */
	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
	this->window = glfwCreateWindow(this->windowSize.width, this->windowSize.height, "Viewer", NULL, NULL);
	if (!this->window){
		#ifdef VERBOSE
				fputs("\tFail to open window\n", stderr);
		#endif

		gl_destroy(this);
		return NULL;
	}
	glfwMakeContextCurrent(this->window);
	glfwSwapInterval(0);

	/* init GLEW */
	GLenum glewInitError = glewInit();
	if (glewInitError != GLEW_OK) {
		#ifdef VERBOSE
				fprintf(stderr, "\tFail init GLEW: %s\n", glewGetErrorString(glewInitError));
		#endif

		gl_destroy(this);
		return NULL;
	}

	/* GLFW event control */
	glfwSetWindowCloseCallback(this->window, gl_windowCloseCallback);
	glfwSetErrorCallback(gl_glfwErrorCallback);
	glDebugMessageCallback(gl_glErrorCallback, 0);

	/* OpenGL config */
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
//	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	/* Draw window - mesh */
	GLuint vao, vbo, ebo;
	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);
	glGenBuffers(1, &ebo);
	glBindVertexArray(vao);

	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	GLfloat vertices[] = {
		+1.0f, +1.0f, 1.0f, 0.0f,
		+1.0f, -1.0f, 1.0f, 1.0f,
		-1.0f, -1.0f, 0.0f, 1.0f,
		-1.0f, +1.0f, 0.0f, 0.0f
	};
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	GLuint indices[] = {0, 3, 2, 0, 2, 1};
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_DYNAMIC_DRAW);
	
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (GLvoid*)(0));
	glEnableVertexAttribArray(0);
	
	this->windowDefaultBufferMesh = (gl_mesh){.vao = vao, .vbo = vbo, .ebo = ebo, .drawSize = 6};
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	/* Draw window - shader */
	GLuint shaderV = glCreateShader(GL_VERTEX_SHADER);
	const char* vs = "#version 310 es\n" /* Tested, so we do not need to check the compile status */
	"layout (location = 0) in vec4 position;\n"
	"out vec2 textpos;\n"
	"void main() {\n"
	"	gl_Position = vec4(position.x, position.y, 0.0f, 1.0f);\n"
	"	textpos = vec2(position.z, position.w);\n"
	"}\n";
	glShaderSource(shaderV, 1, (const GLchar * const*)&vs, NULL);
	glCompileShader(shaderV);
	
	GLuint shaderF = glCreateShader(GL_FRAGMENT_SHADER);
	const char* fs = "#version 310 es\n"
	"precision mediump float;\n"
	"in vec2 textpos;\n"
	"uniform sampler2D bitmap;\n"
	"out vec4 color;\n"
	"void main() {\n"
	"	color = texture(bitmap, textpos);\n"
	"}\n";
	glShaderSource(shaderF, 1, (const GLchar * const*)&fs, NULL);
	glCompileShader(shaderF);

	this->windowDefaultBufferProgram = glCreateProgram();
	glAttachShader(this->windowDefaultBufferProgram, shaderV);
	glAttachShader(this->windowDefaultBufferProgram, shaderF);
	glLinkProgram(this->windowDefaultBufferProgram);
	glDeleteShader(shaderV);
	glDeleteShader(shaderF);

	/* Init OK */
	return this;
}

void gl_drawStart(GL this) {
	clock_gettime(CLOCK_REALTIME_COARSE, &this->renderLoopStartTime);
	glfwPollEvents();
}

void gl_drawWindow(GL this, gl_obj* texture) {
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, this->windowSize.width, this->windowSize.height);

	glUseProgram(this->windowDefaultBufferProgram);
	
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, *texture);
	
	glBindVertexArray(this->windowDefaultBufferMesh.vao);
	glDrawElements(GL_TRIANGLES, this->windowDefaultBufferMesh.drawSize, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}

uint64_t gl_drawEnd(GL this, const char* title) {
	if (title)
		glfwSetWindowTitle(this->window, title);
	
	glfwSwapBuffers(this->window);

	struct timespec current;
	clock_gettime(CLOCK_REALTIME_COARSE, &current);
	return (current.tv_sec-this->renderLoopStartTime.tv_sec) * 1000000000LLU + (current.tv_nsec-this->renderLoopStartTime.tv_nsec);
}

int gl_close(GL this, int close) {
	if (close > 0)
		glfwSetWindowShouldClose(this->window, GLFW_TRUE);
	else if (close == 0)
		glfwSetWindowShouldClose(this->window, GLFW_FALSE);
	
	return glfwWindowShouldClose(this->window);
}

void gl_destroy(GL this) {
	if (!this)
		return;

	#ifdef VERBOSE
		fputs("Destroy GL class object\n", stdout);
		fflush(stdout);
	#endif

	glDeleteProgram(this->windowDefaultBufferProgram);
	glDeleteVertexArrays(1, &(this->windowDefaultBufferMesh.vao));
	glDeleteBuffers(1, &(this->windowDefaultBufferMesh.vbo));
	glDeleteBuffers(1, &(this->windowDefaultBufferMesh.ebo));

	if (this->window) glfwTerminate();
	free(this);
}

/* == OpenGL routines == [Static] =========================================================== */

/* Load a shader from file to memory, return a pointer to the memory, use free() to free the memory when no longer need */
char* gl_loadFileToMemory(const char* filename, long int* length);

gl_obj gl_loadShader(const char* shaderVertexFile, const char* shaderFragmentFile, const char* paramName[], gl_param* paramId, const size_t paramCount) {
	#ifdef VERBOSE
		fprintf(stdout, "Load shader: V=%s F=%s\n", shaderVertexFile, shaderFragmentFile);
		fflush(stdout);
	#endif

	GLuint shaderV = 0, shaderF = 0, shader = 0;
	char* shaderSrc = NULL;
	GLint shaderCompileStatus;
	long int length;

	/* Vertex shader */

	shaderSrc = gl_loadFileToMemory(shaderVertexFile, &length);
	if (!shaderSrc) {
		#ifdef VERBOSE
			if (length == 0)
				fprintf(stderr, "\tFail to open vertex shader file: %s (errno = %d)\n", shaderVertexFile, errno);
			else if (length < 0)
				fprintf(stderr, "\tFail to get vertex shader length: %s (errno = %d)\n", shaderVertexFile, errno);
			else
				fputs("\tCannot allocate buffer for vertex shader code\n", stderr);
		#endif

		goto gl_loadShader_error;
	}

	shaderV = glCreateShader(GL_VERTEX_SHADER); //Non-zero
	glShaderSource(shaderV, 1, (const GLchar * const*)&shaderSrc, NULL);
	glCompileShader(shaderV);
	glGetShaderiv(shaderV, GL_COMPILE_STATUS, &shaderCompileStatus);
	if (!shaderCompileStatus) {
		#ifdef VERBOSE
			char compileMsg[255];
			glGetShaderInfoLog(shaderV, 255, NULL, compileMsg);
			fprintf(stderr, "\tGL vertex shader error: %s\n", compileMsg);
		#endif

		goto gl_loadShader_error;
	}
	free(shaderSrc);

	/* Fragment shader */

	shaderSrc = gl_loadFileToMemory(shaderFragmentFile, &length);
	if (!shaderSrc) {
		#ifdef VERBOSE
			if (length == 0)
				fprintf(stderr, "\tFail to open fragment shader file: %s (errno = %d)\n", shaderFragmentFile, errno);
			else if (length < 0)
				fprintf(stderr, "\tFail to get fragment shader length: %s (errno = %d)\n", shaderFragmentFile, errno);
			else
				fputs("\tCannot allocate buffer for fragment shader code\n", stderr);
		#endif

		goto gl_loadShader_error;
	}

	shaderF = glCreateShader(GL_FRAGMENT_SHADER); //Non-zero
	glShaderSource(shaderF, 1, (const GLchar * const*)&shaderSrc, NULL);
	glCompileShader(shaderF);
	glGetShaderiv(shaderF, GL_COMPILE_STATUS, &shaderCompileStatus);
	if (!shaderCompileStatus) {
		#ifdef VERBOSE
			char compileMsg[255];
			glGetShaderInfoLog(shaderF, 255, NULL, compileMsg);
			fprintf(stderr, "\tGL fragment shader error: %s\n", compileMsg);
		#endif

		goto gl_loadShader_error;
	}
	free(shaderSrc);

	/* Link program */

	shader = glCreateProgram(); //Non-zero
	glAttachShader(shader, shaderV);
	glAttachShader(shader, shaderF);
	glLinkProgram(shader);
	glGetShaderiv(shader, GL_LINK_STATUS, &shaderCompileStatus);
	if (!shaderCompileStatus) {
		#ifdef VERBOSE
			char compileMsg[255];
			glGetShaderInfoLog(shader, 255, NULL, compileMsg);
			fprintf(stderr, "\tGL shader link error: %s\n", compileMsg);
		#endif
		
		goto gl_loadShader_error;
	}

	for (size_t i = paramCount; i; i--) {
		*paramId = glGetUniformLocation(shader, *paramName);
		#ifdef VERBOSE
			fprintf(stdout, "\tParam '%s' location: %d\n", *paramName, *paramId);
			fflush(stdout);
		#endif
		paramId++;
		paramName++;
	}

	glDeleteShader(shaderV); //Flag set, will be automatically delete when program deleted.
	glDeleteShader(shaderF); //So we don't need to remember the vertex and fragment shaders ptr.
	return shader;

	gl_loadShader_error:
	glDeleteShader(shaderV); //A value of 0 for shader will be silently ignored
	glDeleteShader(shaderF);
	glDeleteProgram(shader); //A value of 0 for program will be silently ignored
	free(shaderSrc);
	return 0;
}

void gl_useShader(gl_obj* shader) {
	glUseProgram(*shader);
}

void gl_setShaderParam_internal(gl_param paramId, uint8_t length, gl_datatype type, void* data) {
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

void gl_unloadShader(gl_obj* shader) {
	glDeleteProgram(*shader);
	*shader = 0;
}

gl_mesh gl_createMesh(size2d_t vertexSize, size_t indexCount, gl_index_t* elementsSize, gl_vertex_t* vertices, gl_index_t* indices) {
	GLuint vao, vbo, ebo;
	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);
	glGenBuffers(1, &ebo);
	glBindVertexArray(vao);

	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, vertexSize.height * vertexSize.width * sizeof(gl_vertex_t), vertices, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexCount * sizeof(gl_index_t), indices, GL_DYNAMIC_DRAW);
	
	GLuint elementIndex = 0, attrIndex = 0;
	while (elementIndex < vertexSize.width) {
		glVertexAttribPointer(attrIndex, *elementsSize, GL_FLOAT, GL_FALSE, vertexSize.width * sizeof(float), (GLvoid*)(elementIndex * sizeof(float)));
		glEnableVertexAttribArray(attrIndex);
		elementIndex += *(elementsSize++);
		attrIndex++;
	}
	
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	return (gl_mesh){
		.vao = vao,
		.vbo = vbo,
		.ebo = ebo,
		.drawSize = indexCount
	};
}

void gl_drawMesh(gl_mesh* mesh) {
	glBindVertexArray(mesh->vao);
	glDrawElements(GL_TRIANGLES, mesh->drawSize, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}

void gl_deleteMesh(gl_mesh* mesh) {
	glDeleteVertexArrays(1, &mesh->vao);
	glDeleteBuffers(1, &mesh->vbo);
	glDeleteBuffers(1, &mesh->ebo);
	mesh->vao = 0;
	mesh->vbo = 0;
	mesh->ebo = 0;
}

gl_obj gl_createTexture(size2d_t size) {
	GLuint text;
	glGenTextures(1, &text);
	glBindTexture(GL_TEXTURE_2D, text);
	
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, size.width, size.height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

//	glBindTexture(GL_TEXTURE_2D, 0);
	return text;
}

void gl_updateTexture(gl_obj* texture, size2d_t size, void* data) {
	glBindTexture(GL_TEXTURE_2D, *texture);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, size.width, size.height, GL_RGB, GL_UNSIGNED_BYTE, data);
//	glBindTexture(GL_TEXTURE_2D, 0);
}

void gl_bindTexture(gl_obj* texture, unsigned int unit) {
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(GL_TEXTURE_2D, *texture);
}

void gl_deleteTexture(gl_obj* texture) {
	glDeleteTextures(1, texture);
	*texture = 0;
}

gl_fb gl_createFrameBuffer(size2d_t size) {
	GLuint frameBuffer;
	glGenFramebuffers(1, &frameBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);

	GLuint textureBuffer = gl_createTexture(size);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureBuffer, 0);

	return (gl_fb){
		.frame = frameBuffer,
		.texture = textureBuffer
	};
}

void gl_bindFrameBuffer(gl_fb* fb, size2d_t size, int clear) {
	glBindFramebuffer(GL_FRAMEBUFFER, fb->frame);

	if (size.width || size.height) {
		glViewport(0, 0, size.width, size.height);
	}

	if (clear) {
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
	}
}

void gl_deleteFrameBuffer(gl_fb* fb) {
	gl_deleteTexture(&fb->texture); //Set texture and frame to 0. If we forget something, the error will be draw on vieweer window, so we can know the issue
	glDeleteFramebuffers(1, &fb->frame);
	fb->frame = 0;
}

/* == Private functions ===================================================================== */

void gl_windowCloseCallback(GLFWwindow* window) {
	fputs("Window close event fired.\n", stdout);
	glfwSetWindowShouldClose(window, GLFW_TRUE);
}

void gl_glfwErrorCallback(int code, const char* desc) {
	fprintf(stderr, "GLFW error %d: %s\n", code, desc);
}

void GLAPIENTRY gl_glErrorCallback(GLenum src, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
	fprintf(stderr, "OpenGL info: %s type %x, severity %x, message: %s\n", (type == GL_DEBUG_TYPE_ERROR ? "ERROR" : ""), type, severity, message);
}

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