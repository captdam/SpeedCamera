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
};

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
	{gl_texformat_R8UI,		GL_R8UI,	GL_RED_INTEGER,		GL_UNSIGNED_BYTE	},
	{gl_texformat_RG8UI,		GL_RG8UI,	GL_RG_INTEGER,		GL_UNSIGNED_BYTE	},
	{gl_texformat_RGB8UI,		GL_RGB8UI,	GL_RGB_INTEGER,		GL_UNSIGNED_BYTE	},
	{gl_texformat_RGBA8UI,		GL_RGBA8UI,	GL_RGBA_INTEGER,	GL_UNSIGNED_BYTE	},
	{gl_texformat_R16F,		GL_R16F,	GL_RED,			GL_FLOAT		},
	{gl_texformat_RG16F,		GL_RG16F,	GL_RG,			GL_FLOAT		},
	{gl_texformat_RGB16F,		GL_RGB16F,	GL_RGB,			GL_FLOAT		},
	{gl_texformat_RGBA16F,		GL_RGBA16F,	GL_RGBA,		GL_FLOAT		},
	{gl_texformat_R32F,		GL_R32F,	GL_RED,			GL_FLOAT		},
	{gl_texformat_RG32F,		GL_RG32F,	GL_RG,			GL_FLOAT		},
	{gl_texformat_RGB32F,		GL_RGB32F,	GL_RGB,			GL_FLOAT		},
	{gl_texformat_RGBA32F,		GL_RGBA32F,	GL_RGBA,		GL_FLOAT		}
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

GL gl_init(size2d_t frameSize, unsigned int windowRatio, float mix) {
	#ifdef VERBOSE
		fputs("Init GL class object\n", stdout);
		fflush(stdout);
	#endif

	/* Program check */
	if (!gl_texformat_lookup_check()) {
		#ifdef VERBOSE
			fputs("\tProgram error: gl_texformat_lookup - Program-time error\n", stderr);
		#endif
		return NULL;
	}

	/* Object init */
	GL this = malloc(sizeof(struct GL_ClassDataStructure));
	if (!this) {
		#ifdef VERBOSE
			fputs("\tFail to create gl class object data structure\n", stderr);
		#endif
		return NULL;
	}
	this->window = NULL;
	this->defaultMesh = GL_INIT_DEFAULT_MESH;
	this->defaultShader = GL_INIT_DEFAULT_SHADER;

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
//	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
	#ifdef VERBOSE
//		glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
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
		GLint queue_int;

		const GLenum queueParamInt[] = {
			GL_MAX_TEXTURE_SIZE,
			GL_MAX_ARRAY_TEXTURE_LAYERS
		};
		const char* queueTextInt[] = {
			"Max texture size",
			"Max array texture layers"
		};
		for (size_t i = 0; i < sizeof(queueParamInt) / sizeof(queueParamInt[0]); i++) {
			glGetIntegerv(queueParamInt[i], &queue_int);
			fputc('\t', stdout);
			fputs(queueTextInt[i], stdout);
			fprintf(stdout, ": %d\n", queue_int);
		}

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
	glfwSetWindowCloseCallback(this->window, gl_windowCloseCallback);
	glfwSetErrorCallback(gl_glfwErrorCallback);
	glDebugMessageCallback(gl_glErrorCallback, NULL);

	/* OpenGL config */
//	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
//	glPixelStorei(GL_UNPACK_ROW_LENGTH, frameSize.width);
//	glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, frameSize.height);
//	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	/* Draw window - mesh */ {
		gl_vertex_t vertices[] = {
			1.0f, 0.0f,
			1.0f, 1.0f,
			0.0f, 1.0f,
			0.0f, 0.0f
		};
		gl_index_t indices[] = {0, 3, 2, 0, 2, 1};
		gl_index_t attributes[] = {2};
		this->defaultMesh = gl_mesh_create((size2d_t){.height = 4, .width = 2}, 6, attributes, vertices, indices);
	}

	/* Draw window - shader */ {
		const char* vs = "#version 310 es\nprecision mediump float;\n"
		"layout (location = 0) in vec2 position;\n"
		"out vec2 textpos;\n"
		"void main() {\n"
		"	gl_Position = vec4(position.x * 2.0 - 1.0, position.y * 2.0 - 1.0, 0.0, 1.0);\n"
		"	textpos = vec2(position.x, 1.0 - position.y); //Fix y-axis difference screen coord\n"
		"}\n";

		const char* fs = "#version 310 es\nprecision mediump float;\n"
		"in vec2 textpos;\n"
		"uniform sampler2D orginalTexture;\n"
		"uniform sampler2D processedTexture;\n"
		"uniform float mixLevel;\n"
		"out vec4 color;\n"
		"void main() {\n"
		"	vec3 od = texture(orginalTexture, textpos).rgb;\n"
		"	vec3 pd = texture(processedTexture, textpos).rgb;\n"
//		"	vec3 pd = texture(processedTexture, textpos).rrr;\n"
		"	vec3 data = mix(od, pd, mixLevel);\n"
		"	color = vec4(data, 1.0);\n"
		"}\n";

		const char* gs = NULL;

		const char* pName[] = {"orginalTexture", "processedTexture", "mixLevel"};
		unsigned int pCount = sizeof(pName) / sizeof(pName[0]);
		gl_param pId[pCount];

		const char* bName[] = {};
		unsigned int bCount = sizeof(bName) / sizeof(bName[0]);
		gl_param bId[bCount];

		this->defaultShader = gl_shader_load(vs, fs, gs, 0, pName, pId, pCount, bName, bId, bCount);
		if (this->defaultShader == GL_INIT_DEFAULT_SHADER) {
			#ifdef VERBOSE
				fputs("\tFail to load default shader\n", stderr);
			#endif

			gl_destroy(this);
			return NULL;
		}

		gl_shader_use(&this->defaultShader);
		gl_shader_setParam(pId[2], 1, gl_type_float, &mix);

		this->defaultShader_paramOrginal = pId[0];
		this->defaultShader_paramProcessed = pId[1];
	}

	/* Init OK */
	return this;
}

void gl_drawStart(GL this, size2d_t* cursorPos) {
	glfwPollEvents();

	double x, y;
	glfwGetCursorPos(this->window, &x, &y);
	*cursorPos = (size2d_t){.x = x, .y = y}; //Cast from double to size_t
}

void gl_drawWindow(GL this, gl_tex* orginalTexture, gl_tex* processedTexture) {
	gl_fb df = {.frame=0};
	gl_frameBuffer_bind(&df, this->windowSize, 0);

	gl_shader_use(&this->defaultShader);
	gl_texture_bind(orginalTexture, this->defaultShader_paramOrginal, 0);
	gl_texture_bind(processedTexture, this->defaultShader_paramProcessed, 1);
	
	gl_mesh_draw(&this->defaultMesh);
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

	gl_shader_unload(&this->defaultShader);
	gl_mesh_delete(&this->defaultMesh);

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

gl_shader gl_shader_load(
	const char* shaderVertex, const char* shaderFragment, const char* shaderGeometry, const int isFilePath,
	const char* paramName[], gl_param* paramId, const unsigned int paramCount,
	const char* blockName[], gl_param* blockId, const unsigned int blockCount
) {
	#ifdef VERBOSE
		if (isFilePath) {
			fprintf(stdout, "Load shader: V=%s F=%s G=%s\n", shaderVertex, shaderFragment, shaderGeometry);
		}
		else {
			fputs("Load shader: V=[in_memory] F=[in_memory] G=[in_memory]\n", stdout);
		}
		fflush(stdout);
	#endif

	GLuint shaderV = 0, shaderF = 0, shaderG = 0, shader = 0;
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
	
	/* Geometry shader */
	if (shaderGeometry) {
		if (isFilePath) {
			long int length;
			shaderSrc = gl_loadFileToMemory(shaderGeometry, &length);
			if (!shaderSrc) {
				#ifdef VERBOSE
					if (length == 0)
						fprintf(stderr, "\tFail to open geometry shader file: %s (errno = %d)\n", shaderGeometry, errno);
					else if (length < 0)
						fprintf(stderr, "\tFail to get geometry shader length: %s (errno = %d)\n", shaderGeometry, errno);
					else
						fputs("\tCannot allocate buffer for geometry shader code\n", stderr);
				#endif

				goto gl_loadShader_error;
			}
		}
		else
			shaderSrc = shaderGeometry;

		shaderG = glCreateShader(GL_GEOMETRY_SHADER); //Non-zero
		glShaderSource(shaderG, 1, (const GLchar * const*)&shaderSrc, NULL);
		glCompileShader(shaderG);
		glGetShaderiv(shaderG, GL_COMPILE_STATUS, &status);
		if (!status) {
			#ifdef VERBOSE
				char compileMsg[255];
				glGetShaderInfoLog(shaderF, 255, NULL, compileMsg);
				fprintf(stderr, "\tGL geometry shader error: %s\n", compileMsg);
			#endif

			goto gl_loadShader_error;
		}

		if (isFilePath)
			free((void*)shaderSrc);
	}

	/* Link program */

	shader = glCreateProgram(); //Non-zero
	glAttachShader(shader, shaderV);
	glAttachShader(shader, shaderF);
	if (shaderGeometry) glAttachShader(shader, shaderG);
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
		#ifdef VERBOSE_SHADERPARAM
			fprintf(stdout, "\tParam '%s' location: %d\n", *paramName, *paramId);
			fflush(stdout);
		#endif
		paramId++;
		paramName++;
	}

	for (size_t i = blockCount; i; i--) {
		*blockId = glGetUniformBlockIndex(shader, *blockName);
		#ifdef VERBOSE_SHADERPARAM
			fprintf(stdout, "\tBlock '%s' location: %d\n", *blockName, *paramId);
			fflush(stdout);
		#endif
		blockId++;
		blockName++;
	}

	glDeleteShader(shaderV); //Flag set, will be automatically delete when program deleted.
	glDeleteShader(shaderF); //So we don't need to remember the vertex and fragment shaders name.
	return shader;

	gl_loadShader_error:
	if (shaderV) glDeleteShader(shaderV); //A value of 0 for shader will be silently ignored
	if (shaderF) glDeleteShader(shaderF);
	if (shaderG) glDeleteShader(shaderG);
	if (shader) glDeleteProgram(shader); //A value of 0 for program will be silently ignored
	return GL_INIT_DEFAULT_SHADER;
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

void gl_shader_unload(gl_shader* shader) {
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

int gl_mesh_check(gl_mesh* mesh) {
	if (mesh->vao == GL_INIT_DEFAULT_MESH.vao)
		return 0;
	if (mesh->vbo == GL_INIT_DEFAULT_MESH.vbo)
		return 0;
	if (mesh->ebo == GL_INIT_DEFAULT_MESH.ebo)
		return 0;
	return 1;
}

void gl_mesh_draw(gl_mesh* mesh) {
	glBindVertexArray(mesh->vao);
	glDrawElements(GL_TRIANGLES, mesh->drawSize, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
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
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

	glBindTexture(GL_TEXTURE_2D, GL_INIT_DEFAULT_TEX);
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

void gl_texture_bind(gl_tex* texture, gl_param paramId, unsigned int unit) {
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(GL_TEXTURE_2D, *texture);
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