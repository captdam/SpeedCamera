#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

#include "common.h"
#include "source.h"
#include "filter.h"
#include "compare.h"

#define DEBUG_STAGE 3

#define FOV_H 57.5
#define FOV_V 32.3
#define INSTALL_HEIGHT 10.0
#define INSTALL_PITCH -14.0 //0 = horizon; 90 = sky; -90 = ground;
#define SPEED_MAX 120.0 //Max possible speed in km/h

void __swapBuffer(void** a, void** b) {
	void* temp = *a;
	*a = *b;
	*b = temp;
}
#define swapBuffer(a, b) __swapBuffer((void*)a, (void*)b)

#if DEBUG_STAGE != 0
FILE* debug;
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
			if (d < 128)
				putc(0, debug);
			else if (d < 192)
				putc((d - 128) << 2, debug);
			else
				putc(255, debug);
			if (d < 64)
				putc(d << 2, debug);
			else if (d < 192)
				putc(255, debug);
			else
				putc((255 - d) << 2, debug);
			if (d < 64)
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

int main(int argc, char* argv[]) {
	int status = EXIT_FAILURE;
	size_t frameCount;
	vh_t videoInfo;
	size2d_t size;
	
	Source source = NULL;
	Compare compare = NULL;

	source = source_init(argv[1], &videoInfo);
	if (!source) {
		fputs("Fail to init source input.\n", stderr);
		goto label_exit;
	}
	if (videoInfo.colorScheme != 1 && videoInfo.colorScheme != 3) {
		fprintf(stderr, "Bad color scheme, support 1 or 3 only, got %"PRIu16".\n", videoInfo.colorScheme);
		goto label_exit;
	}
	size = (size2d_t){.width=videoInfo.width, .height=videoInfo.height};

	loc3d_t* locationMap = malloc(sizeof(loc3d_t) * size.width * size.height);
	if (!locationMap) {
		fputs("Fail to init location map.\n", stderr);
		goto label_exit;
	}
	loc3d_t* locationMapWritePtr = locationMap;
	double pitchTop = (90.0 + INSTALL_PITCH + 0.5*FOV_V) * M_PI / 180;
	double pitchBottom = (90.0 + INSTALL_PITCH - 0.5*FOV_V) * M_PI / 180;
	double pitchStep = (pitchBottom - pitchTop) / (size.height - 1);
	double pitchCurrent = pitchTop;
	for (size_t y = 0; y < size.height; y++) {
		double yawSpan = INSTALL_HEIGHT / cos(pitchCurrent) * sin(0.5 * FOV_H * M_PI / 180); //Half
		double yawStep = 2 * yawSpan / (size.width - 1);
		double yawCurrent = -yawSpan;
		float posY = tan(pitchCurrent) * INSTALL_HEIGHT;
		for (size_t x = 0; x < size.width; x++) {
			*(locationMapWritePtr++) = (loc3d_t){
				.x = yawCurrent,
				.y = posY,
				.z = 0
			};
			yawCurrent += yawStep;
		}
		pitchCurrent += pitchStep;
	}
	compare = compare_init(size, locationMap, SPEED_MAX / (3.6 * videoInfo.fps));
	free(locationMap);

	uint8_t* bufferNew = malloc(size.height * size.width);
	uint8_t* bufferOld = malloc(size.height * size.width);

#if DEBUG_STAGE != 0
	debug = fopen("../../debugspace/debug.data", "wb");
	if (!debug) {
		fputs("Error, cannot init debug file.\n", stderr);
		goto label_exit;
	}
	fprintf(stdout, "Debug stage %d, see result in file\n", DEBUG_STAGE);
	fprintf(stdout, "Media info: %"PRIu16" * %"PRIu16", %"PRIu16"\n", videoInfo.width, videoInfo.height, videoInfo.fps);
	vh_t debugInfo = {.width=videoInfo.width, .height=videoInfo.height, .fps=videoInfo.fps, .colorScheme=3};
	fwrite(&debugInfo, sizeof(debugInfo), 1, debug);
#endif

	for (frameCount = 0; source_read(source, bufferNew); frameCount++) { //Read frame from source
#if DEBUG_STAGE != 0
		fprintf(stdout, "\rProgress: %zu", frameCount);
		fflush(stdout);
#endif
		/* Load frame from source */
#if DEBUG_STAGE == 1
		dumpFrame(bufferOld, size, 0);
#endif

		/* Apply edge detection */
		swapBuffer(&bufferNew, &bufferOld);
		filter_kernel3(bufferNew, bufferOld, size, filter_kernel3_edge4);
//		swapBuffer(&bufferNew, &bufferOld);
//		filter_kernel3(bufferNew, bufferOld, size, filter_kernel3_blur);
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
	compare_destroy(compare);

	fprintf(stdout, "\r%zu frames processed.\n\n", frameCount);
	return status;
}