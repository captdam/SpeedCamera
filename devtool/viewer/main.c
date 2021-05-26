#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

#include <GL/glew.h>
#include <GL/glfw3.h>

const char* vs = "#version 330 core\n\
\n\
layout (location = 0) in vec4 position;\n\
\n\
out vec2 textpos;\n\
\n\
void main() {\n\
	gl_Position = vec4(position.x, position.y, 0.0f, 1.0f);\n\
	textpos = vec2(position.z, position.w);\n\
}\n\
";
const char* fs = "#version 330 core\n\
\n\
in vec2 textpos;\n\
\n\
uniform sampler2D bitmap;\n\
\n\
out vec4 color;\n\
\n\
void main() {\n\
	color = texture(bitmap, textpos);\n\
}\n\
";

/* Video file header */
typedef struct VideoHeader_t {
	uint16_t width;
	uint16_t height;
	uint16_t fps;
	uint16_t colorScheme;
} vh_t;

int main(int argc, char* argv[]) {
	if (argc < 2) {
		fputs("Bad command, use: ./this inputFileName", stderr);
		return EXIT_FAILURE;
	}
	const char* inputFileName = argv[1];

	FILE* fp = fopen(inputFileName, "rb");
	if (!fp) {
		fputs("Cannot open input file.\n", stderr);
		return EXIT_FAILURE;
	}
	vh_t header;
	int iDontCareYourResult = fread(&header, 1, sizeof(header), fp);
	const size_t width = header.width;
	const size_t height = header.height;
	const size_t fps = header.fps;
	const size_t colorScheme = header.colorScheme;

	const char* colorSchemeDesc[] = {"Mono/Gray","","RGB","RGBA"};
	if (colorScheme != 1 && colorScheme != 3 && colorScheme != 4) {
		fputs("Bad file header: bad color scheme", stderr);
		return EXIT_FAILURE;
	}

	fprintf(stdout, "Read bitmap from '%s', resolution = %zu * %zu %s, FPS = %zu\n", inputFileName, width, height, colorSchemeDesc[colorScheme-1], fps);

	uint8_t* bitmap = malloc(width * height * colorScheme);
	if (!bitmap) {
		fputs("Cannot allocate memory for bitmap buffer.\n", stderr);
		fclose(fp);
		return EXIT_FAILURE;
	}

	if (!glfwInit()) {
		fputs("GLFW init fail.\n", stderr);
		fclose(fp);
		free(bitmap);
		return EXIT_FAILURE;
	}
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
	
	GLFWwindow* window = glfwCreateWindow(width, height, "Viewer", NULL, NULL);
	if (!window){
		fputs("Window init fail.\n", stderr);
		fclose(fp);
		free(bitmap);
		glfwTerminate();
		return EXIT_FAILURE;
	}
	glfwMakeContextCurrent(window);

	GLenum glewInitError = glewInit();
	if (glewInitError != GLEW_OK) {
		fprintf(stderr, "GLEW init fail: %s\n", glewGetErrorString(glewInitError));
		fclose(fp);
		free(bitmap);
		glfwTerminate();
		return EXIT_FAILURE;
	}

	//Window close event (alt-F4 or right-top cornor exit button)
	void windowCloseCallback(GLFWwindow* window) {
		fputs("Window close event fired.\n", stdout);
		glfwSetWindowShouldClose(window, GLFW_TRUE);
	}
	glfwSetWindowCloseCallback(window, windowCloseCallback);

	//GLFW error log - Not fatal, no need to halt main program
	void glfwErrorCallback(int code, const char* desc) {
		fprintf(stderr, "GLFW error %d: %s\n", code, desc);
	}
	glfwSetErrorCallback(glfwErrorCallback);

	//OpenGL debug log - Not fatal, no need to halt main program
	void GLAPIENTRY glErrorCallback(GLenum src, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
		fprintf(stderr, "OpenGL info: %s type 0x%x, severity 0x%x, message: %s\n", (type == GL_DEBUG_TYPE_ERROR ? "ERROR" : ""), type, severity, message);
	}
	glDebugMessageCallback(glErrorCallback, 0);

	glViewport(0, 0, width, height); //glfwSetWindowSize at graph_frame 1 will trigger windowResize event which calls this
//	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	GLuint shaderV, shaderF, shader;
	GLint shaderCompileStatus;
	shaderV = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(shaderV, 1, &vs, NULL);
	glCompileShader(shaderV);
	glGetShaderiv(shaderV, GL_COMPILE_STATUS, &shaderCompileStatus);
	if (!shaderCompileStatus) {
		fputs("GL vertex shader error.\n", stderr);
		fclose(fp);
		free(bitmap);
		glfwTerminate();
		return EXIT_FAILURE;
	}
	shaderF = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(shaderF, 1, &fs, NULL);
	glCompileShader(shaderF);
	glGetShaderiv(shaderF, GL_COMPILE_STATUS, &shaderCompileStatus);
	if (!shaderCompileStatus) {
		fputs("GL fragment shader error.\n", stderr);
		fclose(fp);
		free(bitmap);
		glfwTerminate();
		return EXIT_FAILURE;
	}
	shader = glCreateProgram();
	glAttachShader(shader, shaderV);
	glAttachShader(shader, shaderF);
	glLinkProgram(shader);
	glGetShaderiv(shader, GL_LINK_STATUS, &shaderCompileStatus);
	if (!shaderCompileStatus) {
		fputs("GL shader link error.\n", stderr);
		fclose(fp);
		free(bitmap);
		glfwTerminate();
		return EXIT_FAILURE;
	}
	glDeleteShader(shaderV);
	glDeleteShader(shaderF);

	GLfloat vertices[][4] = {
		/*tr*/ {+1.0f, +1.0f, 1.0f, 0.0f},
		/*br*/ {+1.0f, -1.0f, 1.0f, 1.0f},
		/*bl*/ {-1.0f, -1.0f, 0.0f, 1.0f},
		/*tl*/ {-1.0f, +1.0f, 0.0f, 0.0f}
	};
	GLuint indices[] = {0, 3, 2, 0, 2, 1};

	GLuint vao, vbo, ebo;
	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);
	glGenBuffers(1, &ebo);
	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(vertices[0]), (GLvoid*)0);
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	GLuint glBitmap;
	glGenTextures(1, &glBitmap);
	glBindTexture(GL_TEXTURE_2D, glBitmap);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST); //Window is not resizeable
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, bitmap);
	glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);

	size_t frame = 0;
	double lastFrameTime = 0;
	glfwSetWindowTitle(window, "Viewer - frame 0");
	while(!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		double cursorX, cursorY;
		uint8_t r, g, b;
		glfwGetCursorPos(window, &cursorX, &cursorY);
		if (cursorX >= 0 && cursorX < width && cursorY >= 0 && cursorY < height && frame > 0) { //Get pixel info if cursor in window
			if (colorScheme == 1) {
				size_t i = cursorY * width + cursorX;
				r = bitmap[i];
				g = r;
				b = r;
			}
			else if (colorScheme == 3) {
				size_t i = (cursorY * width + cursorX) * 3;
				r = bitmap[i+0];
				g = bitmap[i+1];
				b = bitmap[i+2];
			}
			else if (colorScheme == 4) {
				size_t i = (cursorY * width + cursorX) * 4;
				r = bitmap[i+0];
				g = bitmap[i+1];
				b = bitmap[i+2];
			}
		}
		else { //Read next frame wehn cursor out of window
			r = 0;
			g = 0;
			b = 0;
			frame++;
			int notEndOfVideo = fread(bitmap, 1, width * height * colorScheme, fp);
			if (!notEndOfVideo)
				glfwSetWindowShouldClose(window, GLFW_TRUE);
		}

		glUseProgram(shader);
		glBindTexture(GL_TEXTURE_2D, glBitmap);
		if (colorScheme == 1)
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_LUMINANCE, GL_UNSIGNED_BYTE, bitmap);
		else if (colorScheme == 3)
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, bitmap);
		else
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, bitmap);
		glGenerateMipmap(GL_TEXTURE_2D);
		glBindVertexArray(vao);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

		glBindVertexArray(0);
		glBindTexture(GL_TEXTURE_2D, 0);

		char title[40];
		sprintf(title, "Viewer - frame %zu, RGB = %"PRIu8" ,%"PRIu8" ,%"PRIu8, frame, r, g, b);
		glfwSetWindowTitle(window, title);
		glfwSwapBuffers(window);

		double wait = 1.0 / fps - (glfwGetTime() - lastFrameTime);
		if (wait > 0)
			usleep(wait * 1000000);
		lastFrameTime = glfwGetTime();
	}

	glDeleteVertexArrays(1, &vao);
	glDeleteBuffers(1, &vbo);
	glDeleteBuffers(1, &ebo);
	glDeleteTextures(1, &glBitmap);
	glDeleteProgram(shader);

	fclose(fp);
	free(bitmap);
	glfwTerminate();
	fprintf(stdout, "%zu frames displayed.\n\n", frame);
	return EXIT_SUCCESS;
}