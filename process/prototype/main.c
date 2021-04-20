#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "common.h"
#include "source.h"
#include "edge.h"
#include "project.h"
#include "compare.h"

int main(int argc, char* argv[]) {
	int status = EXIT_SUCCESS;
	size_t frameCount = 0;
	FILE* debug = fopen("edge.data", "wb");

	const char* sourceFile = argv[3];
	size2d_t cameraSize = {
		.width = atoi(argv[1]),
		.height = atoi(argv[2])
	};

	Source source = NULL;
	Edge edge = NULL;
	Project project = NULL;
	Compare compare = NULL;

	source = source_init(sourceFile, cameraSize, 3);
	if (!source) {
		fputs("Fail to init source input.\n", stderr);
		goto label_initfail;
	}

	edge = edge_init(source_getRawBitmap(source), cameraSize, 3);
	if (!edge) {
		fputs("Fail to init edge filter.\n", stderr);
		goto label_initfail;
	}

	project = project_init(edge_getEdgeImage(edge), cameraSize, 60.0f, 33.75f, 10.0f, 10.0f);
	if (!project) {
		fputs("Fail to init image projecter.\n", stderr);
		goto label_initfail;
	}
	size2d_t projectSize = project_getProjectSize(project);
	printf("Projection field is width %zu * height %zu\n", projectSize.width, projectSize.height);

	compare = compare_init(project_getProjectImage(project), projectSize, NULL);
	if (!compare) {
		fputs("Fail to init comparator.\n", stderr);
		goto label_initfail;
	}

	for (;frameCount < 120; frameCount++) { //Read frame from source
		source_read(source);

		//Apply edge detection filter
		edge_process(edge);
//		fwrite(edge_getEdgeImage(edge), sizeof(luma_t), (cameraSize.width-2) * (cameraSize.height-2), debug);

		//Project edges from camera-domain (clip-space, shifted by wind) to viewer-domain (stable) based on accelerometer
		project_process(project, 0, 0);
		//TODO: Read gyro
		//NOTE: Pass the project limit to compare object. When camera shifting, edge of frme may becomes 0, this will trigger a
		//mistake "object-duration-move-out" event
//		fwrite(project_getProjectImage(project), sizeof(luma_t), projectSize.width * projectSize.height, debug);

		//Analysis speed base on screen-domain--world-domain info and time of edge luma stay on slots of screen domain
		compare_process(compare);
		fwrite(compare_getSpeedMap(compare), sizeof(uint8_t), projectSize.width * projectSize.height, debug);
	}

	goto label_exit;
label_initfail:
	status = EXIT_FAILURE;
label_exit:
	source_destroy(source);
	edge_destroy(edge);
	project_destroy(project);
	compare_destroy(compare);
	fclose(debug);
	fprintf(stdout, "%zu frames processed.\n\n", frameCount);
	return status;
}