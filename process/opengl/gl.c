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
	gl_mesh windowDefaultBufferMesh;
	gl_shader windowDefaultBufferShader;
	size2d_t windowSize;
};

/* == Window management and driver init == [Object] ========================================= */

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
	this->window = NULL;
	this->windowDefaultBufferMesh = GL_INIT_DEFAULT_MESH;
	this->windowDefaultBufferShader = GL_INIT_DEFAULT_SHADER;

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
	#ifdef VERBOSE
		glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
	#endif
	this->windowSize = (size2d_t){.width = frameSize.width / windowRatio, .height = frameSize.height / windowRatio};
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

	#ifdef VERBOSE 
		fprintf(stdout, "OpenGL driver: %s\n", glGetString(GL_VERSION));
		fflush(stdout);
	#endif

	/* init GLEW */
	GLenum glewInitError = glewInit();
	if (glewInitError != GLEW_OK) {
		#ifdef VERBOSE
			fprintf(stderr, "\tFail init GLEW: %s\n", glewGetErrorString(glewInitError));
		#endif
		gl_destroy(this);
		return NULL;
	}

	/* Event control - callback */
	glfwSetWindowCloseCallback(this->window, gl_windowCloseCallback);
	glfwSetErrorCallback(gl_glfwErrorCallback);
	glDebugMessageCallback(gl_glErrorCallback, NULL);

	/* OpenGL config */
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
//	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	/* Draw window - mesh */
	gl_vertex_t vertices[] = {
		+1.0f, +1.0f, 1.0f, 0.0f,
		+1.0f, -1.0f, 1.0f, 1.0f,
		-1.0f, -1.0f, 0.0f, 1.0f,
		-1.0f, +1.0f, 0.0f, 0.0f
	};
	gl_index_t indices[] = {0, 3, 2, 0, 2, 1};
	gl_index_t attributes[] = {4};
	this->windowDefaultBufferMesh = gl_mesh_create((size2d_t){4,4}, 6, attributes, vertices, indices);

	/* Draw window - shader */
	const char* vs = "#version 310 es\n"
	"layout (location = 0) in vec4 position;\n"
	"out vec2 textpos;\n"
	"void main() {\n"
	"	gl_Position = vec4(position.x, position.y, 0.0f, 1.0f);\n"
	"	textpos = vec2(position.z, position.w);\n"
	"}\n";
	const char* fs = "#version 310 es\n"
	"precision mediump float;\n"
	"in vec2 textpos;\n"
	"uniform sampler2D bitmap;\n"
	"out vec4 color;\n"
	"void main() {\n"
	"	float red = texture(bitmap, textpos).r;\n"
	"	color = vec4(red, red, red, 1.0f);\n"
	"}\n";
	this->windowDefaultBufferShader = gl_shader_load(vs, fs, NULL, NULL, 0, 0);
	if (!this->windowDefaultBufferShader) {
		#ifdef VERBOSE
			fputs("\tFail to load default shader\n", stderr);
		#endif

		gl_destroy(this);
		return NULL;
	}

	/* Init OK */
	return this;
}

void gl_drawStart(GL this) {
	glfwPollEvents();
}

void gl_drawWindow(GL this, gl_tex* texture) {
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, this->windowSize.width, this->windowSize.height);

	glUseProgram(this->windowDefaultBufferShader);
	
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, *texture);
	
	glBindVertexArray(this->windowDefaultBufferMesh.vao);
	glDrawElements(GL_TRIANGLES, this->windowDefaultBufferMesh.drawSize, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}

void gl_drawEnd(GL this, const char* title) {
	if (title)
		glfwSetWindowTitle(this->window, title);
	
	glfwSwapBuffers(this->window);
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

	gl_close(this, 1);

	gl_shader_unload(&this->windowDefaultBufferShader);
	gl_mesh_delete(&this->windowDefaultBufferMesh);

	if (this->window)
		glfwTerminate();
	free(this);
}

/* == OpenGL routines == [Static] =========================================================== */

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

gl_shader gl_shader_load(const char* shaderVertex, const char* shaderFragment, const char* paramName[], gl_param* paramId, const unsigned int paramCount, const int isFilePath) {
	#ifdef VERBOSE
		if (isFilePath) {
			fprintf(stdout, "Load shader: V=%s F=%s\n", shaderVertex, shaderFragment);
		}
		else {
			fputs("Load shader: V=[in_memory] F[in_memory]\n", stdout);
		}
		fflush(stdout);
	#endif

	GLuint shaderV = 0, shaderF = 0, shader = 0;
	const char* shaderSrc = NULL;
	GLint status;

	/* Vertex shader */
	if (isFilePath) {
		long int length;
		shaderSrc = gl_loadFileToMemory(shaderVertex, &length);
		if (!shaderSrc) {
			#ifdef VERBOSE
				if (length == 0)
					fprintf(stderr, "\tFail to open vertex shader file: %s (errno = %d)\n", shaderVertex, errno);
				else if (length < 0)
					fprintf(stderr, "\tFail to get vertex shader length: %s (errno = %d)\n", shaderVertex, errno);
				else
					fputs("\tCannot allocate buffer for vertex shader code\n", stderr);
			#endif

			goto gl_loadShader_error;
		}
	}
	else
		shaderSrc = shaderVertex;

	shaderV = glCreateShader(GL_VERTEX_SHADER); //Non-zero
	glShaderSource(shaderV, 1, (const GLchar * const*)&shaderSrc, NULL);
	glCompileShader(shaderV);
	glGetShaderiv(shaderV, GL_COMPILE_STATUS, &status);
	if (!status) {
		#ifdef VERBOSE
			char compileMsg[255];
			glGetShaderInfoLog(shaderV, 255, NULL, compileMsg);
			fprintf(stderr, "\tGL vertex shader error: %s\n", compileMsg);
		#endif

		goto gl_loadShader_error;
	}

	if (isFilePath)
		free((void*)shaderSrc);

	/* Fragment shader */
	if (isFilePath) {
		long int length;
		shaderSrc = gl_loadFileToMemory(shaderFragment, &length);
		if (!shaderSrc) {
			#ifdef VERBOSE
				if (length == 0)
					fprintf(stderr, "\tFail to open fragment shader file: %s (errno = %d)\n", shaderFragment, errno);
				else if (length < 0)
					fprintf(stderr, "\tFail to get fragment shader length: %s (errno = %d)\n", shaderFragment, errno);
				else
					fputs("\tCannot allocate buffer for fragment shader code\n", stderr);
			#endif

			goto gl_loadShader_error;
		}
	}
	else
		shaderSrc = shaderFragment;

	shaderF = glCreateShader(GL_FRAGMENT_SHADER); //Non-zero
	glShaderSource(shaderF, 1, (const GLchar * const*)&shaderSrc, NULL);
	glCompileShader(shaderF);
	glGetShaderiv(shaderF, GL_COMPILE_STATUS, &status);
	if (!status) {
		#ifdef VERBOSE
			char compileMsg[255];
			glGetShaderInfoLog(shaderF, 255, NULL, compileMsg);
			fprintf(stderr, "\tGL fragment shader error: %s\n", compileMsg);
		#endif

		goto gl_loadShader_error;
	}

	if (isFilePath)
		free((void*)shaderSrc);

	/* Link program */

	shader = glCreateProgram(); //Non-zero
	glAttachShader(shader, shaderV);
	glAttachShader(shader, shaderF);
	glLinkProgram(shader);
	glGetProgramiv(shader, GL_LINK_STATUS, &status);
	if (!status) {
		#ifdef VERBOSE
			char compileMsg[255];
			glGetProgramInfoLog(shader, 255, NULL, compileMsg);
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
	if (shaderV) glDeleteShader(shaderV); //A value of 0 for shader will be silently ignored
	if (shaderF) glDeleteShader(shaderF);
	if (shader) glDeleteProgram(shader); //A value of 0 for program will be silently ignored
	return GL_INIT_DEFAULT_SHADER;
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

void gl_shader_unload(gl_shader* shader) {
	if (*shader != GL_INIT_DEFAULT_SHADER)
		glDeleteProgram(*shader);
	*shader = GL_INIT_DEFAULT_SHADER;
}

gl_mesh gl_mesh_create(size2d_t vertexSize, size_t indexCount, gl_index_t* elementsSize, gl_vertex_t* vertices, gl_index_t* indices) {
	gl_mesh mesh = GL_INIT_DEFAULT_MESH;
	mesh.drawSize = indexCount;

	glGenVertexArrays(1, &mesh.vao);
	glGenBuffers(1, &mesh.vbo);
	glGenBuffers(1, &mesh.ebo);
	glBindVertexArray(mesh.vao);

	glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
	glBufferData(GL_ARRAY_BUFFER, vertexSize.height * vertexSize.width * sizeof(gl_vertex_t), vertices, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexCount * sizeof(gl_index_t), indices, GL_STATIC_DRAW);
	
	GLuint elementIndex = 0, attrIndex = 0;
	while (elementIndex < vertexSize.width) {
		glVertexAttribPointer(attrIndex, *elementsSize, GL_FLOAT, GL_FALSE, vertexSize.width * sizeof(float), (GLvoid*)(elementIndex * sizeof(float)));
		glEnableVertexAttribArray(attrIndex);
		elementIndex += *(elementsSize++);
		attrIndex++;
	}
	
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	return mesh;
}

void gl_mesh_draw(gl_mesh* mesh) {
	glBindVertexArray(mesh->vao);
	glDrawElements(GL_TRIANGLES, mesh->drawSize, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}

void gl_mesh_delete(gl_mesh* mesh) {
	if (mesh->vbo != GL_INIT_DEFAULT_MESH.vao)
		glDeleteVertexArrays(1, &mesh->vao);
	if (mesh->vbo != GL_INIT_DEFAULT_MESH.vbo)
		glDeleteVertexArrays(1, &mesh->vbo);
	if (mesh->vbo != GL_INIT_DEFAULT_MESH.ebo)
		glDeleteVertexArrays(1, &mesh->ebo);
	*mesh = GL_INIT_DEFAULT_MESH;
}

gl_tex gl_texture_create(size2d_t size) {
	gl_tex texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, size.width, size.height, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	return texture;
}

void gl_texture_update(gl_tex* texture, size2d_t size, void* data) {
	glBindTexture(GL_TEXTURE_2D, *texture);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, size.width, size.height, GL_RED, GL_UNSIGNED_BYTE, data);
}

void gl_texture_bind(gl_tex* texture, unsigned int unit) {
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(GL_TEXTURE_2D, *texture);
}

void gl_texture_delete(gl_tex* texture) {
	if (*texture != GL_INIT_DEFAULT_TEX)
		glDeleteTextures(1, texture);
	*texture = GL_INIT_DEFAULT_TEX;
}

gl_fb gl_frameBuffer_create(size2d_t size) {
	gl_fb buffer = GL_INIT_DEFAULT_FB;

	glGenFramebuffers(1, &buffer.frame);
	glBindFramebuffer(GL_FRAMEBUFFER, buffer.frame);

	buffer.texture = gl_texture_create(size);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, buffer.texture, 0);

	return buffer;
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