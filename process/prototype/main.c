#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "common.h"
#include "source.h"
#include "edge.h"

#define WIDTH 1920
#define HEIGHT 1080

int main() {
	Source* source = source_init("../../bmpv.data", 1920, 1080);
	if (!source) {
		fputs("Fail to init source input.\n", stderr);
		return EXIT_FAILURE;
	}

	luma_t* edgeImg = malloc(sizeof(luma_t) * (1920-2) * (1080-2));
	if (!edgeImg) {
		fputs("Fail to init luma buffer.\n", stderr);
		return EXIT_FAILURE;
	}

	FILE* edgeImgFile = fopen("edge.data", "wb");

	for (size_t i = 0; i < 30; i++) {
		//Read frame from source
		source_read(source);

		//Apply edge detection filter
		edge(source_getRawBitmap(source), edgeImg, 1920, 1080);
		fwrite(edgeImg, sizeof(luma_t), (1920-2) * (1080-2), edgeImgFile);

		//Project edges from camera window (clip-space) to screen-domain (sphere unclippe space) based on accele meter

		//Analysis speed base on screen-domain--world-domain info and time of edge luma stay on slots of screen domain
	}

	source_destroy(source);
	fclose(edgeImgFile);
	return EXIT_SUCCESS;
} 