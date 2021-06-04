#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

#include "common.h"
#include "source.h"
#include "roadmap.h"
#include "filter.h"
#include "compare.h"

#define DEBUG_STAGE 2
#define DEBUG_FILE_DIR "./debugspace/debug.data"

void __swapBuffer(void** a, void** b);
#define swapBuffer(a, b) __swapBuffer((void*)a, (void*)b)

#if DEBUG_STAGE != 0
FILE* debug;
void dumpFrame(uint8_t* frame, size2d_t size, int hue);
#endif

int main(int argc, char* argv[]) {
	int status = EXIT_FAILURE;
	size_t frameCount = 0;
	vh_t videoInfo;
	size2d_t size;

	Source source = NULL;
	Roadmap roadmap = NULL;
	Compare compare = NULL;

	source = source_init(argv[1], &videoInfo);
	if (!source) {
		fputs("Fail to init source input.\n", stderr);
		goto label_exit;
	}
	if (videoInfo.colorScheme != 1 && videoInfo.colorScheme != 3 && videoInfo.colorScheme != 4) {
		fprintf(stderr, "Bad color scheme, support 1, 3 and 4 only, got %"PRIu16".\n", videoInfo.colorScheme);
		goto label_exit;
	}
	size = (size2d_t){.width=videoInfo.width, .height=videoInfo.height};

	roadmap = roadmap_init(argv[2], size);
	if (!roadmap) {
		fputs("Fail to init roadmap.\n", stderr);
		goto label_exit;
	}
	road_t* roadmapRoadInfo = roadmap_getRoadpoint(roadmap);

	compare = compare_init(size, roadmapRoadInfo);
	if (!compare) {
		fputs("Fail to init compare module.\n", stderr);
		goto label_exit;
	}

	uint8_t* bufferNew = malloc(size.height * size.width * sizeof(uint8_t));
	uint8_t* bufferOld = malloc(size.height * size.width * sizeof(uint8_t));

#if DEBUG_STAGE != 0
	debug = fopen(DEBUG_FILE_DIR, "wb");
	if (!debug) {
		fputs("Error, cannot init debug file.\n", stderr);
		goto label_exit;
	}
	fprintf(stdout, "Debug stage %d, see result in file\n", DEBUG_STAGE);
	fprintf(stdout, "\tMedia info: %"PRIu16" * %"PRIu16", %"PRIu16"\n", videoInfo.width, videoInfo.height, videoInfo.fps);
	vh_t debugInfo = {.width=videoInfo.width, .height=videoInfo.height, .fps=videoInfo.fps, .colorScheme=3};
	fflush(stdout);
	fwrite(&debugInfo, sizeof(debugInfo), 1, debug);
#endif

	for (; source_read(source, bufferNew); frameCount++) { //Read frame from source
#ifdef VERBOSE
		fprintf(stdout, "\rProgress: %zu", frameCount);
		fflush(stdout);
#endif
		/* Load frame from source */
#if DEBUG_STAGE == 1
		dumpFrame(bufferNew, size, 0);
#endif

		/* Apply edge detection */
		swapBuffer(&bufferNew, &bufferOld);
		filter_kernel3(bufferNew, bufferOld, size, filter_kernel3_gaussian);
		swapBuffer(&bufferNew, &bufferOld);
		filter_kernel3(bufferNew, bufferOld, size, filter_kernel3_edge4);
#if DEBUG_STAGE == 2
		dumpFrame(bufferNew, size, 0);
#endif

		/* Compare new and old frame to detect moving */
		swapBuffer(&bufferNew, &bufferOld);
		compare_process(compare, bufferNew, bufferOld);
#if DEBUG_STAGE == 3
		dumpFrame(bufferNew, size, 1);
#endif
	}



	
	status = EXIT_SUCCESS;
label_exit:
#if DEBUG_STAGE != 0
	if (debug) fclose(debug);
#endif
	source_destroy(source);
	roadmap_destroy(roadmap);
	compare_destroy(compare);

	fprintf(stdout, "\r%zu frames processed.\n\n", frameCount);
	return status;
}

void __swapBuffer(void** a, void** b) {
	void* temp = *a;
	*a = *b;
	*b = temp;
}

#if DEBUG_STAGE != 0
void dumpFrame(uint8_t* frame, size2d_t size, int hue) {
	size_t area = size.width * size.height;
	if (hue) {
		for (size_t i = area; i; i--) {
			uint8_t d = *(frame++);
			if (!d) {
				putc(0, debug);
				putc(0, debug);
				putc(0, debug);
				continue;
			}
			if (d < 128) //R
				putc(0, debug);
			else if (d < 192)
				putc((d - 128) << 2, debug);
			else
				putc(255, debug);
			if (d < 64) //G
				putc(d << 2, debug);
			else if (d < 192)
				putc(255, debug);
			else
				putc((255 - d) << 2, debug);
			if (d < 64) //B
				putc(255, debug);
			else if (d < 128)
				putc((128 - d) << 2, debug);
			else
				putc(0, debug);
		}
	}
	else {
		for (size_t i = area; i; i--) {
			uint8_t d = *(frame++);
			fputc(d, debug);
			fputc(d, debug);
			fputc(d, debug);
		}
	}
}
#endif