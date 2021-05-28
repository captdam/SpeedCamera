/** This program is used to generate road map file
 * A road map file contains infomation about which pixel should the main program look when it 
 * detects a moving pixel.
 * This program should be run for once before launch the main program.
 * This program is intended to be run on dev machine.
 * This program doest not free resource on error termination, the OS takes care of it.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <inttypes.h>

#define FOV_H 57.5
#define FOV_V 32.3
#define INSTALL_HEIGHT 10.0
#define INSTALL_PITCH -14.0 //0 = horizon; 90 = sky; -90 = ground;
#define SPEED_MAX 120.0 //Max possible speed in km/h

typedef struct Location3D {
	double x, y, z;
} loc3d_t;

typedef struct Roadmap {
	unsigned int distance: 8;
	unsigned int pos: 24;
} map_t;
#define ROADMAP_DIS_MAX 255
#define ROADMAP_POS_MAX 16777215

double distanceLoc3d(loc3d_t a, loc3d_t b) {
	return sqrt( powf(a.x-b.x,2) + powf(a.y-b.y,2) + powf(a.z-b.z,2) );
}

int main(int argc, char* argv[]) {
	const char* outputFilename = argv[1];
	const int width = atoi(argv[2]); //Use int, even int16 (+/-32k) is large enough.
	const int height = atoi(argv[3]); //Plus, our target machine is 32-bit machine
	const double fovH = atof(argv[4]);
	const double fovV = atof(argv[5]);
	const double installHeight = atof(argv[6]);
	const double installPitch = atof(argv[7]);
	const double maxSpeed = atof(argv[8]);
	const double fps = atof(argv[9]);
	fprintf(stdout, "Export roadmap to file %s\n", outputFilename);
	fprintf(stdout, "Screen width = %d, height = %d\n", width, height);
	fprintf(stdout, "Camera FOV = horizontal %.2lf deg * vertical %.2lf deg\n", fovH, fovV);
	fprintf(stdout, "Install height %.2lf m, pitch %.2lf deg\n", installHeight, installPitch);
	fprintf(stdout, "Max speed %.2lf km/h, FPS = %.2lf\n", maxSpeed, fps);

	if (width * height > ROADMAP_POS_MAX) {
		fprintf(stderr, "Resolution too large! (Limit = %llu)\n", (unsigned long long)ROADMAP_POS_MAX);
		return EXIT_FAILURE;
	}

	double distanceThreshold = maxSpeed / 3.6 / fps;
	fprintf(stdout, "Max speed is %.2lf m/s, so threshold is %.2lf/frame at %.2f FPS\n", maxSpeed/3.6, distanceThreshold, fps);

	/* Create map file */
	FILE* fp = fopen(argv[1], "wb");
	if (!fp) {
		fputs("Cannot create file.", stdout);
		return EXIT_FAILURE;
	}

	/* Create road info */
	loc3d_t* locationMap = malloc(sizeof(loc3d_t) * width * height);
	if (!locationMap) {
		fputs("Fail to init location map.\n", stderr);
		return EXIT_FAILURE;
	}
	loc3d_t* locationMapWritePtr = locationMap;
	double pitchTop = (90.0 + installPitch + 0.5*fovV) * M_PI / 180;
	double pitchBottom = (90.0 + installPitch - 0.5*fovV) * M_PI / 180;
	double pitchStep = (pitchBottom - pitchTop) / (height - 1);
	double pitchCurrent = pitchTop;
	for (int y = 0; y < height; y++) {
		double yawSpan = installHeight / cos(pitchCurrent) * sin(0.5 * fovH * M_PI / 180); //Half
		double yawStep = 2 * yawSpan / (width - 1);
		double yawCurrent = -yawSpan;
		double posY = tan(pitchCurrent) * installHeight;
		for (int x = 0; x < width; x++) {
			*(locationMapWritePtr++) = (loc3d_t){
				.x = yawCurrent,
				.y = posY,
				.z = 0
			};
			yawCurrent += yawStep;
		}
		pitchCurrent += pitchStep;
	}

	/* Find neighbor points */
	uint32_t mapCount = 0;
	map_t* map = malloc(0);
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			uint32_t currentBase = mapCount;
			loc3d_t selfPos = locationMap[y * width + x];
			fprintf(stdout, "\rMap size = %09"PRIu32"\t\tCurrent (%04d,%04d) pos (%03.5f,%03.5f,%03.5f)", mapCount, x, y, selfPos.x, selfPos.y, selfPos.z);
			fflush(stdout);
			fwrite(&currentBase, 1, sizeof(currentBase), fp);

			//Search up/down/left/right
			for (int cx = x - 1;; cx--) {
				if (cx < 0) break;
				uint32_t pos = y * width + cx;
				double distance = distanceLoc3d(selfPos, locationMap[pos]);
				if (distance > distanceThreshold) //Excess threshold
					break;
				map = realloc(map, (mapCount+1) * sizeof(map_t)); //Resize array and push
				if (!map) {
					fputs("\nHalt! Out of memory!\n", stderr);
					return EXIT_FAILURE;
				}
				map[mapCount++] = (map_t){
					.distance = distance / distanceThreshold * ROADMAP_DIS_MAX,
					.pos = pos
				};
			}
			for (int cx = x + 1;; cx++) {
				if (cx >= width) break;
				uint32_t pos = y * width + cx;
				double distance = distanceLoc3d(selfPos, locationMap[pos]);
				if (distance > distanceThreshold) //Excess threshold
					break;
				map = realloc(map, (mapCount+1) * sizeof(map_t));
				if (!map) {
					fputs("\nHalt! Out of memory!\n", stderr);
					return EXIT_FAILURE;
				}
				map[mapCount++] = (map_t){
					.distance = distance / distanceThreshold * ROADMAP_DIS_MAX,
					.pos = pos
				};
			}
			for (int cy = y - 1;; cy--) {
				if (cy < 0) break;
				uint32_t pos = cy * width + x;
				double distance = distanceLoc3d(selfPos, locationMap[pos]);
				if (distance > distanceThreshold) //Excess threshold
					break;
				map = realloc(map, (mapCount+1) * sizeof(map_t));
				if (!map) {
					fputs("\nHalt! Out of memory!\n", stderr);
					return EXIT_FAILURE;
				}
				map[mapCount++] = (map_t){
					.distance = distance / distanceThreshold * ROADMAP_DIS_MAX,
					.pos = pos
				};
			}
			for (int cy = y + 1;; cy++) {
				if (cy >= height) break;
				uint32_t pos = cy * width + x;
				double distance = distanceLoc3d(selfPos, locationMap[pos]);
				if (distance > distanceThreshold) //Excess threshold
					break;
				map = realloc(map, (mapCount+1) * sizeof(map_t));
				if (!map) {
					fputs("\nHalt! Out of memory!\n", stderr);
					return EXIT_FAILURE;
				}
				map[mapCount++] = (map_t){
					.distance = distance / distanceThreshold * ROADMAP_DIS_MAX,
					.pos = pos
				};
			}

			uint32_t currentCount = mapCount - currentBase;
			fwrite(&currentCount, 1, sizeof(currentCount), fp);

			//Sort candidate array by distance
			if (currentCount > 1) {
				for (size_t i = currentBase; i < mapCount - 1; i++) {
					for (size_t j = i + 1; j < mapCount; j++) {
						if (map[j].distance < map[i].distance) {
							map_t temp = map[i];
							map[i] = map[j];
							map[j] = temp;
						}
					}
				}
			}
		}
	}
	free(locationMap);
	fprintf(stdout, "\rMap size = %09"PRIu32"\t\tSearch done!\n", mapCount);
	fflush(stdout);
	
	fputs("Write map to file.\n", stdout);
	fwrite(&mapCount, 1, sizeof(mapCount), fp);
	fwrite(map, sizeof(*map), mapCount, fp);
	free(map);

	fclose(fp);
	return EXIT_SUCCESS;
}