#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "common.h"
#include "source.h"
#include "edge.h"

int main(int argc, char* argv[]) {
	size_t width = atoi(argv[1]);
	size_t height = atoi(argv[2]);

	Source source = source_init("../../bmpv.data", width, height);
	if (!source) {
		fputs("Fail to init source input.\n", stderr);
		return EXIT_FAILURE;
	}

	Edge edge = edge_init(source_getRawBitmap(source), width, height);
	if (!edge) {
		fputs("Fail to init edge filter.\n", stderr);
		return EXIT_FAILURE;
	}

	FILE* edgeImgFile = fopen("edge.data", "wb");

	for (size_t i = 0; i < 150; i++) {
		//Read frame from source
		source_read(source);

		//Apply edge detection filter
		edge_process(edge);
		fwrite(edge_getEdgeImage(edge), sizeof(luma_t), (width-2) * (height-2), edgeImgFile);

		//Project edges from camera window (clip-space) to screen-domain (sphere unclippe space) based on accele meter

		//Analysis speed base on screen-domain--world-domain info and time of edge luma stay on slots of screen domain
	}

	source_destroy(source);
	edge_destroy(edge);
	fclose(edgeImgFile);
	return EXIT_SUCCESS;
}