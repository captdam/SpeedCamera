#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <math.h>

#include "common.h"
#include "denoise.h"
#include "source.h"
#include "edge.h"
#include "project.h"
#include "compare.h"

#define DEBUG_FILENAME "../../debugspace/debug.data"
#define DEBUG_STAGE 3

#define FOV_H 57.5f
#define FOV_V 32.3f
#define FOV_MH 5.0f
#define FOV_MV 5.0f
#define INSTALL_HEIGHT 7.0f
#define INSTALL_PITCH 0.0f //0 = horizon; 90 = sky; -90 = ground;
#define SPEED_MAX 120 //Max possible speed in km/h
#define SPEED_ALERT 128 //Only show speed faster than this/255 of the SPEED_MAX, should be less than 256. Write 0 to show all

int main(int argc, char* argv[]) {
	int status = EXIT_FAILURE;
	size_t frameCount;

#if DEBUG_STAGE != 0
	FILE* debug = fopen(DEBUG_FILENAME, "wb");
#endif

	const char* sourceFile = argv[1];
	size2d_t cameraSize = {
		.width = atoi(argv[2]),
		.height = atoi(argv[3])
	};
	size_t fps = atoi(argv[4]);

	Source source = NULL;
	Edge edge = NULL;
	Project project = NULL;
	Compare compare = NULL;

	source = source_init(sourceFile, cameraSize, 3);
	if (!source) {
		fputs("Fail to init source input.\n", stderr);
		goto label_exit;
	}

	edge = edge_init(source_getRawBitmap(source), cameraSize, 3);
	if (!edge) {
		fputs("Fail to init edge filter.\n", stderr);
		goto label_exit;
	}
	size2d_t edgeSize = edge_getEdgeSize(edge);

	project = project_init(edge_getEdgeImage(edge), edgeSize, FOV_H, FOV_V, FOV_MH, FOV_MV); //edge image is slightly smaller than the actual FOV, but we can ignor it
	if (!project) {
		fputs("Fail to init image projecter.\n", stderr);
		goto label_exit;
	}
	size2d_t projectSize = project_getProjectSize(project);

	compare = compare_init(project_getProjectImage(project), projectSize, SPEED_MAX, fps);
	if (!compare) {
		fputs("Fail to init comparator.\n", stderr);
		goto label_exit;
	}
	size2d_t compareSize = compare_getMapSize(compare);

	loc3d_t* locationMap = malloc(sizeof(loc3d_t) * compareSize.width * compareSize.height);
	if (!locationMap) {
		fputs("Fail to init location map.\n", stderr);
		goto label_exit;
	}
	loc3d_t* locationMapWritePtr = locationMap;
	double pitchTop = (90.0 - INSTALL_PITCH + 0.5*FOV_V + FOV_MV) * M_PI / 180;
	double pitchBottom = (90.0 - INSTALL_PITCH - 0.5*FOV_V + FOV_MV) * M_PI / 180;
	double pitchStep = (pitchBottom - pitchTop) / (compareSize.height - 1);
	double pitchCurrent = pitchTop;
	for (size_t y = 0; y < compareSize.height; y++) {
		double yawSpan = INSTALL_HEIGHT / cos(pitchCurrent) * sin((0.5*FOV_H + FOV_MH) * M_PI / 180); //Half
		double yawStep = 2 * yawSpan / (compareSize.width - 1);
		double yawCurrent = -yawSpan;
		float posY = tan(pitchCurrent) * INSTALL_HEIGHT;
		for (size_t x = 0; x < compareSize.width; x++) {
			*(locationMapWritePtr++) = (loc3d_t){
				.x = yawCurrent,
				.y = posY,
				.z = 0
			};
			yawCurrent += yawStep;
		}
		pitchCurrent += pitchStep;
	}
	compare_setLocationMap(compare, locationMap);
	free(locationMap);

#if DEBUG_STAGE == 1
	fprintf(stdout, "Saving debug data: edge. Size %zu * %zu\n", edgeSize.width, edgeSize.height);
	uint16_t debugFileHeader[4] = {edgeSize.width, edgeSize.height, fps, 1};
	fwrite(debugFileHeader, 1, sizeof(debugFileHeader), debug);
#elif DEBUG_STAGE == 2
	fprintf(stdout, "Saving debug data: project. Size %zu * %zu\n", projectSize.width, projectSize.height);
	uint16_t debugFileHeader[4] = {projectSize.width, projectSize.height, fps, 1};
	fwrite(debugFileHeader, 1, sizeof(debugFileHeader), debug);
#elif DEBUG_STAGE == 3
	fprintf(stdout, "Saving debug data: projectCompare. Size %zu * %zu\n", projectSize.width, projectSize.height);
	uint16_t debugFileHeader[4] = {projectSize.width, projectSize.height, fps, 1};
	fwrite(debugFileHeader, 1, sizeof(debugFileHeader), debug);
#endif

	for (frameCount = 0; source_read(source); frameCount++) { //Read frame from source
		fprintf(stdout, "\rProgress: %zu", frameCount);
		fflush(stdout);

		//Apply edge detection filter
		edge_process(edge);
#if DEBUG_STAGE == 1
		fwrite(edge_getEdgeImage(edge), sizeof(luma_t), (cameraSize.width-2) * (cameraSize.height-2), debug);
#endif

		//Project edges from camera-domain (clip-space, shifted by wind) to viewer-domain (stable) based on accelerometer
		project_process(project, 0, 0, 0);
		//TODO: Read gyro
		//NOTE: Pass the project limit to compare object. When camera shifting, edge of frme may becomes 0, this will trigger a
		//mistake "object-duration-move-out" event
#if DEBUG_STAGE == 2
		fwrite(project_getProjectImage(project), sizeof(luma_t), projectSize.width * projectSize.height, debug);
#endif

		//Analysis speed base on screen-domain--world-domain info and time of edge luma stay on slots of screen domain
		compare_process(compare);
		denoise_lowpass34(compare_getSpeedMap(compare), projectSize);
#if SPEED_ALERT
		denoise_highValuePass8(compare_getSpeedMap(compare), projectSize, SPEED_ALERT);
#endif
#if DEBUG_STAGE == 3
		fwrite(compare_getSpeedMap(compare), sizeof(uint8_t), projectSize.width * projectSize.height, debug);
#endif
	}
	fputs("\rProgress: Done!\n", stdout);
	fflush(stdout);

	status = EXIT_SUCCESS;
label_exit:
	source_destroy(source);
	edge_destroy(edge);
	project_destroy(project);
	compare_destroy(compare);
#if DEBUG_STAGE != 0
	fclose(debug);
#endif
	fprintf(stdout, "%zu frames processed.\n\n", frameCount);
	return status;
}