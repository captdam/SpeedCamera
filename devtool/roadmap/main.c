/** This program is used to generate road map file, which contains road-domain and screen-domain data in both perpective and orthographic view. 
 * Perspective data shows the info of the raw video; orthographic data shows the info of projected video. 
 * Orthographic projection applied on x-axis, so all objects travels in the direction of the look direction but 
 * with left-right offset will look like traveling vertical in orthographic video, instead of sloped in perspective video. 
 * This simplifies the process of object tracing. 
 * 
 * Format: 
 * 
 * | Header: 
 * - width(i16), height(i16)					//Frame size in pixel, also used to calculate the size of this file. ASCII meta data append can be ignored 
 * - searchThreshold(f32)					//For search distance 
 * - orthoPixelWidth(f32)					//Pixel width is same for all pixels in orthographic view 
 * - searchDistanceX_orthographic(i32)				//Max distance in x-axis in pixel an object can move under the speed threshold, in orthographic view 
 * - Point{roadX(f32), roadY(f32), screenX(i32), screenY(i32)}[4: farLeft, farRight, closeLeft, closeRight] 
 * 								//Reference of how to calculate the orthographic, also marks focus region 
 * 
 * | Table1(f32*4)[height][width]: 
 * - Perspective: roadX(f32), roadY(f32)			//Road-domain geographic data in both view mode 
 * - Orthographic: roadX(f32), roadY(f32) 
 * 
 * | Table2(i32*4)[height][width]: 
 * - SearchDistanceY(i32)					//Max distance in y-axis in pixel an object can move under the speed threshold, same in both view
 * - SearchDistanceX_perspective(i32)				//Max distance in x-axis in pixel an object can move under the speed threshold, in perspective view
 * - lookupYP2O(i32), lookupO2P(i32) 				//Projection lookup table. X-crood in orthographic and perspective views are same 
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <inttypes.h>
#include <errno.h>
#include <assert.h>

//Point on road-domain and screen-domain
typedef struct Point_t {
	float roadX, roadY;
	unsigned int screenX, screenY;
} point_t;

/** Output file fomat:
 * Section I:	Header
 * Section II:	Table 1: Road-domain geographic data
 * Section III:	Table 2: Search distance, P/O projection map
 * Section IV:	Meta data: ASCII meta data, for reference, ignored by program
 */
typedef struct FileHeader {
	int16_t width, height; //Frame size in pixel, also used to calculate the size of this file. ASCII meta data append can be ignored
	float searchThreshold; //Max distance an object can travel between two frame, for search distance
	float orthoPixelWidth; //Pixel width is same for all pixels in orthographic view
	unsigned int searchDistanceXOrtho; //Max distance in x-axis in pixel an object can move under the speed threshold, in orthographic view, same for all pixels
	point_t farLeft, farRight, closeLeft, closeRight; //Points in perspective view, reference of how to calculate the orthographic, also can be used to determine focus region
} header_t;
typedef struct FileDataTable1 { //Road-domain geographic data in both view
	float px, py;
	float ox, oy;
} data1_t;
typedef struct FileDataTable2 {
	unsigned int searchDistanceXPersp; //Max distance in x-axis in pixel an object can move under the speed threshold, in perspective view
	unsigned int searchDistanceY; //Max distance in y-axis in pixel an object can move under the speed threshold, same in both view
	unsigned int lookupXp2o, lookupXo2p; //Projection lookup table. Y-crood in orthographic and perspective views are same
} data2_t;

//Deg to rad
#define d2r(deg) ((deg) * M_PI / 180.0)

//Rad to deg
#define r2d(rad) ((rad) * 180.0 / M_PI)

//Deg sin, cos, tan
#define dsin(deg) (sin(d2r(deg)))
#define dcos(deg) (cos(d2r(deg)))
#define dtan(deg) (tan(d2r(deg)))

//Find distance between two point
double fd(double x1, double y1, double x2, double y2) {
	double dx = x1 - x2, dy = y1 - y2;
	return sqrt(dx * dx + dy * dy);
}

//Return 0 if X is closer to A, 1 if X is closer to B, -1 if X is smaller than A or greater then B
int isBetween(double x, double a, double b) {
	if (a > b)
		assert(a <= b);
	if (x < a || x > b)
		return -1;
	return ((x - a) <= (b - x)) ? 0 : 1;
}

int main(int argc, char* argv[]) {

	/* Cmd argu check */ if (argc != 10) {
		fputs(
			"Bad arg: Use 'this installHeight roadPitch installPitch fovHorizontal fovVertical sizeHorizontal sizeVeritcal threshold filename'\n"
			"\twhere installHeight is height of camera in meter\n"
			"\t      roadPitch is pitch angle of road in degree, incline is positive\n"
			"\t      installPitch is pitch angle of camera in degree, looking sky is positive\n"
			"\t      fovHorizontal is horizontal field of view of camera in degree\n"
			"\t      fovVertical is vertical field of view of camera in degree\n"
			"\t      sizeHorizontal is width of camera image in unit of pixel\n"
			"\t      sizeVeritcal is height of camera image in unit of pixel\n"
			"\t      threshold is the max displacement an object can have between two frame in unit of meter\n"
			"\t      filename is where to save the file\n"
			"\teg; this 10.0 20.0 0.0 57.5 32.3 1920 1080 3.5 roadmap.data\n"
		, stderr);
		return EXIT_FAILURE;
	}
	double installHeight = atof(argv[1]), roadPitch = atof(argv[2]), installPitch = atof(argv[3]), fovHorizontal = atof(argv[4]), fovVertical = atof(argv[5]);
	unsigned int width = atoi(argv[6]), height = atoi(argv[7]);
	double threshold = atof(argv[8]);
	const char* roadmap = argv[9];

	/* Adjust FOV */ {
		if (fovHorizontal > 0.0 && fovVertical > 0.0) {
			fputs("No adjustment required on FOV\n", stdout);
		} else if (fovHorizontal <= 0.0 && fovVertical > 0.0) {
			fovHorizontal = fovVertical * width / height;
			fprintf(stdout, "Adjust horizontal FOV to %lf based on vertical FOV and screen size\n", fovHorizontal);
		} else if (fovVertical <= 0.0 && fovHorizontal > 0.0) {
			fovVertical = fovHorizontal * height / width;
			fprintf(stdout, "Adjust vertical FOV to %lf based on horizontal FOV and screen size\n", fovVertical);
		} else {
			fputs("Bad FOV\n", stderr);
			return EXIT_FAILURE;
		}
	}

	fprintf(stdout, "Camera height: %lf m, pitch: %lf deg\n", installHeight, installPitch);
	fprintf(stdout, "Camera resolution: %u x %u p, FOV: %lf x %lf deg\n", width, height, fovHorizontal, fovVertical);
	fprintf(stdout, "Road picth: %lf deg\n", roadPitch);
	fprintf(stdout, "Threshold: %lf m/frame\n", threshold);
	fflush(stdout);
	
	/* Create file */
	fprintf(stdout, "Creating road map file '%s'\n", roadmap);
	fflush(stdout);
	FILE* fp = fopen(roadmap, "wb");
	if (!fp) {
		fprintf(stderr, "Fail to create/open output file '%s' (errno=%d)\n", roadmap, errno);
		return EXIT_FAILURE;
	}

	header_t header = {
		.width = width, .height = height,
		.searchThreshold = threshold,
		.orthoPixelWidth = 0.0, //TBD
		.searchDistanceXOrtho = 0, //TBD
		.farLeft = {.roadX = 0.0, .roadY = 0.0, .screenX = 0, .screenY = 0}, //TBD
		.farRight = {.roadX = 0.0, .roadY = 0.0, .screenX = 0, .screenY = 0}, //TBD
		.closeLeft = {.roadX = 0.0, .roadY = 0.0, .screenX = 0, .screenY = 0}, //TBD
		.closeRight = {.roadX = 0.0, .roadY = 0.0, .screenX = 0, .screenY = 0} //TBD
	};

	/* Allocate memory */
	fputs("Allocating memory\n", stdout);
	fflush(stdout);
	data1_t (*t1)[width] = malloc(sizeof(data1_t) * height * width);
	data2_t (*t2)[width] = malloc(sizeof(data2_t) * height * width);
	if (!t1 || !t2) {
		fprintf(stderr, "Out of memory (errno=%d)\n", errno);
		fclose(fp);
		if (t1) free(t1);
		if (t2) free(t2);
		return EXIT_FAILURE;
	}

	/* Find road data */ {
		fputs("Finding road geographic data\n", stdout);
		fflush(stdout);

		/* Perspective */ {
			for (unsigned int y = 0; y < height; y++) {
				double lookPitch = installPitch + 0.5 * fovVertical - y / (height - 1.0) * fovVertical;
				double dHeight = roadPitch - lookPitch, dY = 90 + lookPitch, dRoad = 90 - roadPitch;
				double py = (installHeight / dsin(dHeight)) * dsin(dY); //Road y-coord
				double d = (installHeight / dsin(dHeight)) * dsin(dRoad); //Distance from camera to middle of road (x direction = 0 deg)
				
//				if (y % 5 == 0) {
//					printf("Idx %u: LP %.4lf, d1 %.2lf, d2 %.2f, y %.4lf, d %.4lf\n", y, lookPitch, dHeight, dY, py, d);
//				}
				
				for (unsigned int x = 0; x < width; x++) {
					double lookHorizontal = - 0.5 * fovHorizontal + x / (width - 1.0) * fovHorizontal;
					double px = dtan(lookHorizontal) * d;
					t1[y][x].px = px;
					t1[y][x].py = py;
				}
			}
		}

		/* Find 4 corner points */ {
			//Close points always at bottom of screen
			header.closeLeft = (point_t){.screenX = 0, .screenY = height-1, .roadX = t1[height-1][0].px, .roadY = t1[height-1][0].py}; //Left-bottom
			header.closeRight = (point_t){.screenX = width-1, .screenY = height-1, .roadX = t1[height-1][width-1].px, .roadY = t1[height-1][width-1].py}; //Right-bottom

			//Find y-coord of far points
			unsigned int farY;
			if (t1[0][0].py > 0) { //Camera FOV below horizon: far points at top of screen
				farY = 0;
			} else { //Horizon in FOV, find it
				for (unsigned int y = 0; y < height - 1; y++) {
					if (t1[y][0].py < 0 && t1[y+1][0].py > 0) {
						farY = y + 1;
						break;
					}
				}
			}

			//Find far points
			for (unsigned int x = 0; x < width - 1; x++) {
				if (isBetween(header.closeLeft.roadX, t1[farY][x].px, t1[farY][x+1].px) != -1)
					header.farLeft = (point_t){.screenX = x, .screenY = farY, .roadX = t1[farY][x].px, .roadY = t1[farY][x].py};
				if (isBetween(header.closeRight.roadX, t1[farY][x].px, t1[farY][x+1].px) != -1)
					header.farRight = (point_t){.screenX = x+1, .screenY = farY, .roadX = t1[farY][x+1].px, .roadY = t1[farY][x+1].py};
			}
			
		}

		/* Orthographic */ {
			double baselineLeft = t1[height-1][0].px, baselineRight = t1[height-1][width-1].px;
			double xStep = (baselineRight - baselineLeft) / (width - 1.0);
			unsigned int farY = 0;
			for (unsigned int y = 0; y < height; y++) {
				for (unsigned int x = 0; x < width; x++) {
					t1[y][x].ox = baselineLeft + xStep * x;
					t1[y][x].oy = t1[y][x].py; //Y-axis in orthographic view is same as in perspective view (only project in x-axis)
				}
			}

			header.orthoPixelWidth = xStep;
		}

		fprintf(stdout, "\t- Far left:    screen(%"PRIu32", %"PRIu32"), road(%f, %f)\n", header.farLeft.screenX, header.farLeft.screenY, header.farLeft.roadX, header.farLeft.roadY);
		fprintf(stdout, "\t- Far right:   screen(%"PRIu32", %"PRIu32"), road(%f, %f)\n", header.farRight.screenX, header.farRight.screenY, header.farRight.roadX, header.farRight.roadY);
		fprintf(stdout, "\t- Close left:  screen(%"PRIu32", %"PRIu32"), road(%f, %f)\n", header.closeLeft.screenX, header.closeLeft.screenY, header.closeLeft.roadX, header.closeLeft.roadY);
		fprintf(stdout, "\t- Close right: screen(%"PRIu32", %"PRIu32"), road(%f, %f)\n", header.closeRight.screenX, header.closeRight.screenY, header.closeRight.roadX, header.closeRight.roadY);
	}

	/* Calculate search distance */ {
		fputs("Calculating searching distance\n", stdout);
		fflush(stdout);
		for (unsigned int y = 0; y < height; y++) {
			for (unsigned int x = 0; x < width; x++) {
				point_t persp = {.roadX = t1[y][x].px, .roadY = t1[y][x].py};
				unsigned int left = 0, right = 0, up = 0, down = 0;
				while (x - left > 0 && t1[y][x-left].px - persp.roadX <= threshold)
					left++;
				while (x + right < width - 1 && t1[y][x+right].px - persp.roadY <= threshold)
					right++;
				while (y - up > 0 && t1[y-up][x].py - persp.roadY <= threshold)
					up++;
				while (y + down < height - 1 && t1[y+down][x].py - persp.roadY <= threshold)
					down++;

				t2[y][x].searchDistanceXPersp = left > right ? left : right;
				t2[y][x].searchDistanceY = up > down ? up : down;
			}
		}

		header.searchDistanceXOrtho = threshold / header.orthoPixelWidth;
	}

	/* Find lookup table */ {
		fputs("Finding lookup table\n", stdout);
		fflush(stdout);
		for (unsigned int y = 0; y < height; y++) {
			for (unsigned int x = 0; x < width; x++) {
				/* Perspective to Orthographic */ {
					float road = t1[y][x].ox;
					if (road <= t1[y][0].px) {
						t2[y][x].lookupXp2o = 0;
					} else if (road >= t1[y][width-1].px) {
						t2[y][x].lookupXp2o = width - 1;
					} else {
						for (unsigned int i = 0; i < width - 1; i++) {
							float left = t1[y][i].px, right = t1[y][i+1].px;
							int check = isBetween(road, left, right);
							if (check != -1) {
								t2[y][x].lookupXp2o = i + check;
								break;
							}
						}
					}
				}

				/* Orthographic to Perspective */ {
					float road = t1[y][x].px;
					if (road <= t1[y][0].ox) {
						t2[y][x].lookupXo2p = 0;
					} else if (road >= t1[y][width-1].ox) {
						t2[y][x].lookupXo2p = width - 1;
					} else {
						for (unsigned int i = 0; i < width - 1; i++) {
							float left = t1[y][i].ox, right = t1[y][i+1].ox;
							int check = isBetween(road, left, right);
							if (check != -1) {
								t2[y][x].lookupXo2p = i + check;
								break;
							}
						}
					}
				}
			}
		}
	}

	/* Write to file */ {
		fputs("Writing to file\n", stdout);
		fflush(stdout);
		fwrite(&header, sizeof(header), 1, fp);
		fwrite(t1, sizeof(t1[0][0]), height * width, fp);
		fwrite(t2, sizeof(t2[0][0]), height * width, fp);
		fputs("\n\n== Metadata: ===================================================================\n", fp);
		fprintf(fp, "CMD: %s %s %s %s %s %s %s %s\n", argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], argv[7], argv[8]);
		fprintf(fp, "Camera height: %lf m, pitch: %lf deg\n", installHeight, installPitch);
		fprintf(fp, "Camera resolution: %u x %u p, FOV: %lf x %lf deg\n", width, height, fovHorizontal, fovVertical);
		fprintf(fp, "Road picth: %lf deg\n", roadPitch);
		fprintf(fp, "Threshold: %lf m/frame\n", threshold);
	}

	fclose(fp);
	fprintf(stdout, "Roadmap file '%s' generated\n", roadmap);
	fflush(stdout);
	return EXIT_SUCCESS;
}