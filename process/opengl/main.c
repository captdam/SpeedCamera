#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <errno.h>

#include "common.h"
#include "gl.h" 

#define WINDOW_RATIO 1

int main(int argc, char* argv[]) {
	if (argc < 2) {
		fputs("Bad command, use: ./this inputFileName", stderr);
		return EXIT_FAILURE;
	}
	const char* inputFileName = argv[1];

	FILE* fp = fopen(inputFileName, "rb");
	if (!fp) {
		fprintf(stderr, "Cannot open input file (%d).\n", errno);
		return EXIT_FAILURE;
	}
	vh_t header;
	int iDontCareYourResult = fread(&header, 1, sizeof(header), fp);
	const size_t width = header.width;
	const size_t height = header.height;
	const size_t fps = header.fps;
	const size_t colorScheme = header.colorScheme;
//	const size_t width = 1920;
//	const size_t height = 1080;
//	const size_t fps = 30;
//	const size_t colorScheme = 4;

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

	GL gl = gl_init((size2d_t){.width=width, .height=height}, WINDOW_RATIO);
	if (!gl) {
		fputs("Cannot init OpenGL.\n", stderr);
		fclose(fp);
		return EXIT_FAILURE;
	}

	gl_obj shader = gl_loadShader("reg_2d.glsl", "1.fs.glsl");
	if (!shader) {
		fputs("Cannot load program.\n", stderr);
		fclose(fp);
		gl_destroy(gl);
		return EXIT_FAILURE;
	}

	gl_vertex_t vertices[] = {
		/*tr*/ +1.0f, +1.0f, 1.0f, 0.0f,
		/*br*/ +1.0f, -1.0f, 1.0f, 1.0f,
		/*bl*/ -1.0f, -1.0f, 0.0f, 1.0f,
		/*tl*/ -1.0f, +1.0f, 0.0f, 0.0f
	};
	gl_index_t attributes[] = {4};
	gl_index_t indices[] = {0, 3, 2, 0, 2, 1};
	gl_mesh mesh = gl_createMesh((size2d_t){.height=4, .width=4}, 6, attributes, vertices, indices);

	gl_obj texture = gl_createTexture(header, bitmap);


	size_t frame = 0;
	double lastFrameTime = 0;
	while(!gl_close(gl, -1)) {
		if (!fread(bitmap, 1, width * height * colorScheme, fp))
			gl_close(gl, 1);

		gl_drawStart(gl);

		gl_updateTexture(texture, header, bitmap);

		gl_useProgram(shader);
		gl_bindTexture(texture, 0);
		gl_drawMesh(mesh);

		char title[60];
		sprintf(title, "Viewer - frame %zu", frame++);
		gl_drawEnd(gl, title);
	}

	gl_deleteMesh(mesh);
	gl_deleteTexture(texture);
	gl_unloadShader(shader);

	fclose(fp);
	free(bitmap);

	gl_destroy(gl);
	fprintf(stdout, "%zu frames displayed.\n\n", frame);
	return EXIT_SUCCESS;
}