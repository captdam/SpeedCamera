/** This program is used to generate road map file
 * A road map file contains infomation about which pixel should the main program look when it 
 * detects a moving pixel.
 * This program should be run for once before launch the main program.
 * This program is intended to be run on dev machine.
 * This program doest not free resource on error termination, the OS takes care of it.
 * Our dev machine has a powerful and reliable OS.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <inttypes.h>

typedef struct Location3D {
	double x, y, z;
} loc3d_t;

typedef struct RoadPixel {
	uint32_t base;
	uint32_t count;
} road_t;

typedef struct RoadNeighbor {
	unsigned int distance: 8;
	unsigned int pos: 24;
} neighbor_t;
#define ROADMAP_DIS_MAX 255
#define ROADMAP_POS_MAX 16777215

/* == Modify following values and functions to generate road map file for a specific scene == */

//Screen resolution in pixel
#define WIDTH 1920
#define HEIGHT 1080

//Camera field of view (Horizontal and Veritcal) in degree
#define FOVH 57.5
#define FOVV 32.3

//Position of camera: height in meter, pitch in degree, 0 = horizontal, 90 = sky, -90 = ground
#define INSTALL_HEIGHT 10.0
#define INSTALL_PITCH -14.0

//Max speed of object in km/h, Video FPS. Thes two value also used as threshold for searching neighbor points
#define MAX_SPEED 120.0
#define FPS 30.0

/** Road location generation
 * @param x screen coordnate x in pixel of a road point
 * @param y screen coordnate y in pixel of a road point
 * @return x,y,z geographical coordnate of a road point
 */
loc3d_t getLocation(int x, int y) {
	double pitchTop = (90.0 + INSTALL_PITCH + 0.5*FOVV) * M_PI / 180;
	double pitchBottom = (90.0 + INSTALL_PITCH - 0.5*FOVV) * M_PI / 180;
	double pitchStep = (pitchBottom - pitchTop) / (HEIGHT - 1);
	double pitchCurrent = pitchTop + pitchStep * y;

	double yawSpan = INSTALL_HEIGHT / cos(pitchCurrent) * sin(0.5 * FOVH * M_PI / 180); //Half
	double yawStep = 2 * yawSpan / (WIDTH - 1);
	double yawCurrent = -yawSpan + yawStep * x;

	return (loc3d_t){
		.x = yawCurrent,
		.y = tan(pitchCurrent) * INSTALL_HEIGHT,
		.z = 0
	};
}

/** Focus region, movement outside of the focus region will be ignored
 * @param x screen coordnate x in pixel of a road point
 * @param y screen coordnate y in pixel of a road point
 * @return 1 if in focus region , 0 if out
 */
int isFocused(int x, int y) {
	int x1, x2, y1, y2;

	if (y < 400 || y > 1060)
		return 0;
	
	x1 = 940;
	y1 = 400;
	x2 = 410;
	y2 = 1060;
	if ( x < x1 + (y-y1) * (x1-x2)/(y1-y2) )
		return 0;
	
	x1 = 1060;
	y1 = 400;
	x2 = 1496;
	y2 = 656;
	if ( x > x1 + (y-y1) * (x1-x2)/(y1-y2) )
		return 0;
	
	if (x > 1496)
		return 0;

	return 1;
}

/* == Modify above values and functions to generate road map file for a specific scene == */

#if WIDTH * HEIGHT > ROADMAP_POS_MAX
#error "Resolution to large (Limit 24-bit)"
#endif

#define DISTANCE_THRESHOLD (MAX_SPEED / 3.6 / FPS)

double distanceLoc3d(loc3d_t a, loc3d_t b) {
	return sqrt( powf(a.x-b.x,2) + powf(a.y-b.y,2) + powf(a.z-b.z,2) );
}

int main(int argc, char* argv[]) {
	fprintf(stdout, "Max speed is %.2lf km/h, so threshold is %.2lf/frame at %.2f FPS\n", MAX_SPEED, DISTANCE_THRESHOLD, FPS);

	/* Create map file */
	FILE* fp = fopen(argv[1], "wb");
	if (!fp) {
		fputs("Cannot create file.", stdout);
		return EXIT_FAILURE;
	}

	/* Road data */
	road_t* road = malloc(WIDTH * HEIGHT * sizeof(road_t));
	if (!road) {
		fputs("Cannot create road info buffer.", stdout);
		return EXIT_FAILURE;
	}

	/* Create road info */
	loc3d_t* locationMap = malloc(WIDTH * HEIGHT * sizeof(loc3d_t));
	if (!locationMap) {
		fputs("Fail to init location map.\n", stderr);
		return EXIT_FAILURE;
	}
	loc3d_t* locationMapWritePtr = locationMap;
	for (int y = 0; y < HEIGHT; y++) {
		for (int x = 0; x < WIDTH; x++) {
			*(locationMapWritePtr++) = getLocation(x, y);
		}
	}

	/* Find neighbor points */
	uint32_t neighborCount = 0;
	neighbor_t* neighbor = malloc(0);
	for (int y = 0; y < HEIGHT; y++) {
		for (int x = 0; x < WIDTH; x++) {
			//Get info about current road point
			uint32_t currentBase = neighborCount;
			loc3d_t selfPos = locationMap[y * WIDTH + x];
			fprintf(stdout, "\rMap size = %09"PRIu32"\tCurrent (%04d,%04d) pos (%08.2lf, %08.2lf, %08.2lf)", neighborCount, x, y, selfPos.x, selfPos.y, selfPos.z);
			fflush(stdout);

			if (isFocused(x, y)) { //Make sure current road point is in focus region

				//Search up/down/left/right for neighbor points
				for (int cx = x - 1;; cx--) {
					if (cx < 0) //Make sure we are not going out of the frame edge
						break;
					
					uint32_t pos = y * WIDTH + cx;
					double distance = distanceLoc3d(selfPos, locationMap[pos]);
					if (distance > DISTANCE_THRESHOLD) //Stop if excess threshold (further will have greater distance)
						break;
					
					if (!isFocused(cx, y)) //Also make sure we are still in the focus region
						continue; //Focus region may be discontinue, so we continue to check next neighbor point
					
					neighbor = realloc(neighbor, (neighborCount+1) * sizeof(neighbor_t)); //Resize array and push
					if (!neighbor) {
						fputs("\nHalt! Out of memory!\n", stderr);
						return EXIT_FAILURE;
					}
					neighbor[neighborCount++] = (neighbor_t){
						.distance = distance / DISTANCE_THRESHOLD * ROADMAP_DIS_MAX,
						.pos = pos
					};
				}
				for (int cx = x + 1;; cx++) {
					if (cx >= WIDTH)
						break;
					
					uint32_t pos = y * WIDTH + cx;
					double distance = distanceLoc3d(selfPos, locationMap[pos]);
					if (distance > DISTANCE_THRESHOLD)
						break;
					
					if (!isFocused(cx, y))
						continue;
					
					neighbor = realloc(neighbor, (neighborCount+1) * sizeof(neighbor_t));
					if (!neighbor) {
						fputs("\nHalt! Out of memory!\n", stderr);
						return EXIT_FAILURE;
					}
					neighbor[neighborCount++] = (neighbor_t){
						.distance = distance / DISTANCE_THRESHOLD * ROADMAP_DIS_MAX,
						.pos = pos
					};
				}
				for (int cy = y - 1;; cy--) {
					if (cy < 0)
						break;
					
					uint32_t pos = cy * WIDTH + x;
					double distance = distanceLoc3d(selfPos, locationMap[pos]);
					if (distance > DISTANCE_THRESHOLD)
						break;
					
					if (!isFocused(x, cy))
						continue;

					neighbor = realloc(neighbor, (neighborCount+1) * sizeof(neighbor_t));
					if (!neighbor) {
						fputs("\nHalt! Out of memory!\n", stderr);
						return EXIT_FAILURE;
					}
					neighbor[neighborCount++] = (neighbor_t){
						.distance = distance / DISTANCE_THRESHOLD * ROADMAP_DIS_MAX,
						.pos = pos
					};
				}
				for (int cy = y + 1;; cy++) {
					if (cy >= HEIGHT)
						break;
					
					uint32_t pos = cy * WIDTH + x;
					double distance = distanceLoc3d(selfPos, locationMap[pos]);
					if (distance > DISTANCE_THRESHOLD)
						break;
					
					if (!isFocused(x, cy))
						continue;

					neighbor = realloc(neighbor, (neighborCount+1) * sizeof(neighbor_t));
					if (!neighbor) {
						fputs("\nHalt! Out of memory!\n", stderr);
						return EXIT_FAILURE;
					}
					neighbor[neighborCount++] = (neighbor_t){
						.distance = distance / DISTANCE_THRESHOLD * ROADMAP_DIS_MAX,
						.pos = pos
					};
					double sss = DISTANCE_THRESHOLD;
				}
			}

			uint32_t currentCount = neighborCount - currentBase;
			road[y * WIDTH + x] = (road_t){
				.base = currentBase,
				.count = currentCount
			};

			//Sort neighbor points base on distance, closest first
			if (currentCount > 1) {
				for (size_t i = currentBase; i < neighborCount - 1; i++) {
					for (size_t j = i + 1; j < neighborCount; j++) {
						if (neighbor[j].distance < neighbor[i].distance) {
							neighbor_t temp = neighbor[i];
							neighbor[i] = neighbor[j];
							neighbor[j] = temp;
						}
					}
				}
			}
		}
	}
	free(locationMap);
	fprintf(stdout, "\rMap size = %09"PRIu32"\t\tSearch done!\n", neighborCount);
	fflush(stdout);
	
	fputs("Write map to file.\n", stdout);
	fwrite(&neighborCount, 1, sizeof(neighborCount), fp);
	fwrite(road, sizeof(*road), WIDTH * HEIGHT, fp);
	free(road);
	fwrite(neighbor, sizeof(*neighbor), neighborCount, fp);
	free(neighbor);

	fclose(fp);
	return EXIT_SUCCESS;
}