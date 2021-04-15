#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "common.h"
#include "source.h"
#include "edge.h"
#include "project.c"

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

	Project project = project_init(width, height, 60.0f, 33.75f, 10.0f, 10.0f);

	size_t proWidth, proHeight;
	project_getProjectSize(project, &proWidth, &proHeight);
	printf("Projection field is width %zu * height %zu", proWidth, proHeight);

	FILE* debug = fopen("edge.data", "wb");

	for (size_t i = 0; i < 1; i++) {
		//Read frame from source
		source_read(source);

		//Apply edge detection filter
		edge_process(edge);
//		fwrite(edge_getEdgeImage(edge), sizeof(luma_t), (width-2) * (height-2), edgeImgFile);

		//Project edges from camera window (clip-space) to screen-domain (sphere unclippe space) based on accele meter
		project_write(project, 20, 0, edge_getEdgeImage(edge));
		fwrite(project_getProjectLuma(project), sizeof(luma_t), proWidth * proHeight, debug);
		//TODO: Read gyro

		//Analysis speed base on screen-domain--world-domain info and time of edge luma stay on slots of screen domain
	}

	source_destroy(source);
	edge_destroy(edge);
	project_destroy(project);
	fclose(debug);
	return EXIT_SUCCESS;
}