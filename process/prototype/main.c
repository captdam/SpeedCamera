#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "common.h"
#include "source.h"
#include "edge.h"
#include "project.h"

int main(int argc, char* argv[]) {
	size2d_t cameraSize = {
		.width = atoi(argv[1]),
		.height = atoi(argv[2])
	};

	Source source = source_init("../../bmpv.data", cameraSize, 3);
	if (!source) {
		fputs("Fail to init source input.\n", stderr);
		return EXIT_FAILURE;
	}

	Edge edge = edge_init(source_getRawBitmap(source), cameraSize, 3);
	if (!edge) {
		fputs("Fail to init edge filter.\n", stderr);
		source_destroy(source);
		return EXIT_FAILURE;
	}

	Project project = project_init(edge_getEdgeImage(edge), cameraSize, 60.0f, 33.75f, 10.0f, 10.0f);
	if (!project) {
		fputs("Fail to init image projecter.\n", stderr);
		source_destroy(source);
		edge_destroy(edge);
		return EXIT_FAILURE;
	}

	size2d_t projectSize = project_getProjectSize(project);
	printf("Projection field is width %zu * height %zu", projectSize.width, projectSize.height);

	FILE* debug = fopen("edge.data", "wb");

	for (size_t i = 0; i < 10; i++) {
		//Read frame from source
		source_read(source);

		//Apply edge detection filter
		edge_process(edge);
//		fwrite(edge_getEdgeImage(edge), sizeof(luma_t), (cameraSize.width-2) * (cameraSize.height-2), debug);

		//Project edges from camera-domain (clip-space, shifted by wind) to viewer-domain (stable) based on accelerometer
		project_process(project, 20, -15);
		fwrite(project_getProjectImage(project), sizeof(luma_t), projectSize.width * projectSize.height, debug);
		//TODO: Read gyro
		//NOTE: Pass the project limit to compare object. When camera shifting, edge of frme may becomes 0, this will trigger a
		//mistake "object-duration-move-out" event

		//Analysis speed base on screen-domain--world-domain info and time of edge luma stay on slots of screen domain
	}

	source_destroy(source);
	edge_destroy(edge);
	project_destroy(project);
	fclose(debug);
	return EXIT_SUCCESS;
}