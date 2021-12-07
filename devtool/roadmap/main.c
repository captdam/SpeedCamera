/** This program is used to generate road map file, which contains road-domain and screen-domain data in both perpective and orthographic view. 
 * Perspective data shows the info of the raw video; orthographic data shows the info of projected video. 
 * Orthographic projection applied on x-axis, so all objects travels in the direction of the look direction but 
 * with left-right offsets will look like traveling vertical in orthographic video, instead of sloped in perspective video. 
 * This simplifies the process of object tracing. 
 */

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <errno.h>
#include <math.h>
#include <string.h>

/** Output file fomat: 
 * Section I:	Header 
 * Section II:	Table 1: Road-domain geographic data 
 * Section III:	Table 2: Search distance, P/O projection map 
 * Section IV:	Number of points pair(uint), Focus region points (left-to-right, top-to-bottom) 
 * Section V:	Meta data: ASCII meta data, for reference, ignored by program 
 */
typedef struct FileHeader {
	int16_t width, height; //Frame size in pixel, also used to calculate the size of this file. ASCII meta data append can be ignored
	float searchThreshold; //Max distance an object can travel between two frame, for search distance
	float orthoPixelWidth; //Pixel width is same for all pixels in orthographic view
	unsigned int searchDistanceXOrtho; //Max distance in x-axis in pixel an object can move under the speed threshold, in orthographic view, same for all pixels
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
typedef struct Point_t { //Point on road-domain and screen-domain
	float roadX, roadY;
	unsigned int screenX, screenY;
} point_t;

//Temp point_t: data from info file
typedef struct Point_temp_t {
	float roadX1, roadX2;
	unsigned int screenX1, screenX2, screenY;
} ptemp_t;

#define NON_NUMBER_STR "%*[^0-9.-]"

//Deg to rad
#define d2r(deg) ((deg) * M_PI / 180.0)

//Rad to deg
#define r2d(rad) ((rad) * 180.0 / M_PI)

//Deg sin, cos, tan
#define dsin(deg) (sin(d2r(deg)))
#define dcos(deg) (cos(d2r(deg)))
#define dtan(deg) (tan(d2r(deg)))

//Get array length (stack array only, no array pointer)
#define arrayLength(x) (sizeof(x) / sizeof(x[0]))

//Find distance between two point
double fd(double x1, double y1, double x2, double y2) {
	double dx = x1 - x2, dy = y1 - y2;
	return sqrt(dx * dx + dy * dy);
}

//Return 0 if X is closer to A, 1 if X is closer to B, -1 if X is smaller than A or greater then B
int isBetween(double x, double a, double b) {
	if (a > b) {
		fprintf(stderr, "Fail - isBetween(): x = %lf, a,b = %lf, %lf\n", x, a, b);
		exit(EXIT_FAILURE);
	} else if (x < a || x > b)
		return -1;
	return ((x - a) <= (b - x)) ? 0 : 1;
}

//Special fix: find perspective slope equation. Give two points(x,y), return x = (y + offset) * slope + gain
void findSlopeEq(
	point_t higher, point_t lower, 
	double* slopeScreen, double* offsetScreen, double* gainScreen, 
	double* slopeRoad, double* offsetRoad, double* gainRoad 
) {
	*slopeScreen =  ((double)(lower.screenX) - (double)(higher.screenX)) / ((double)(lower.screenY) - (double)(higher.screenY));
	*offsetScreen = -(double)higher.screenY; //Since: slope = (x-x1) / (y-y1) = (x-x2) / (y-y2) = (x2-x1) / (y2-y1)
	*gainScreen = (double)higher.screenX;    //Therefore: offset = -y1, gain = x1 => x = (y-y1) * slope + x1 = (y + offset) * slope + gain
	*slopeRoad =  ((double)(lower.roadX) - (double)(higher.roadX)) / ((double)(lower.screenY) - (double)(higher.screenY)); //screenY / roadX
	*offsetRoad = -(double)higher.screenY;
	*gainRoad = (double)higher.roadX;
}

int main(int argc, char* argv[]) {
	if (argc != 3) {
		fputs("ERROR: Bad arg: Use 'this info.txt output.data\n", stderr);
		fputs("\tinfo.txt is an ASCII file contains human-readable road info", stderr);
		fputs("\toutput.data is a binary file used to tell the program road info", stderr);
		return EXIT_FAILURE;
	}

	/* Get config from file */
	unsigned int pCnt = 0;
	ptemp_t* pTemp = NULL;
	double installHeight = 0.0, installPitch = 0.0;
	unsigned int width = 0, height = 0;
	double fovHorizontal = 0.0, fovVertical = 0.0;
	double roadPitch = 0.0;
	double threshold = 0.0;
	{
		FILE* fp = fopen(argv[1], "r");
		if (!fp) {
			fprintf(stderr, "ERROR: Cannot open file: %s (errno=%d)\n", argv[1], errno);
			return EXIT_FAILURE;
		}

		char buffer[500];
		unsigned int pTempIdx = 0;
		while (fgets(buffer, arrayLength(buffer), fp)) {
			#define strToken(token, str) strncmp(token, str, arrayLength(token) - 1)
			if (!strToken("POINTS", buffer)) {
				sscanf(buffer, NON_NUMBER_STR"%u", &pCnt);
				fprintf(stdout, "INFO: Point counts: %u\n", pCnt);
				pTemp = malloc(sizeof(ptemp_t) * pCnt);
				if (!pTemp) {
					fflush(stdout);
					fprintf(stderr, "ERROR: Cannot allocate memory for temp points storage when reading info file (errno=%d)\n", errno);
					fclose(fp);
					return EXIT_FAILURE;
				}
			} else if (!strToken("INSTALL_HEIGHT", buffer)) {
				sscanf(buffer, NON_NUMBER_STR"%lf", &installHeight);
				fprintf(stdout, "INFO: Install height (m): %lf\n", installHeight);
			} else if (!strToken("INSTALL_PITCH", buffer)) {
				sscanf(buffer, NON_NUMBER_STR"%lf", &installPitch);
				fprintf(stdout, "INFO: Install pitch (deg): %lf\n", installPitch);
			} else if (!strToken("WIDTH", buffer)) {
				sscanf(buffer, NON_NUMBER_STR"%u", &width);
				fprintf(stdout, "INFO: Width (px): %u\n", width);
			} else if (!strToken("HEIGHT", buffer)) {
				sscanf(buffer, NON_NUMBER_STR"%u", &height);
				fprintf(stdout, "INFO: Height (px): %u\n", height);
			} else if (!strToken("FOV_HORIZONTAL", buffer)) {
				sscanf(buffer, NON_NUMBER_STR"%lf", &fovHorizontal);
				fprintf(stdout, "INFO: FOV horizontal (deg): %lf\n", fovHorizontal);
			} else if (!strToken("FOV_VERTICAL", buffer)) {
				sscanf(buffer, NON_NUMBER_STR"%lf", &fovVertical);
				fprintf(stdout, "INFO: FOV vertical (deg): %lf\n", fovVertical);
			} else if (!strToken("ROAD_PITCH", buffer)) {
				sscanf(buffer, NON_NUMBER_STR"%lf", &roadPitch);
				fprintf(stdout, "INFO: Road pitch (deg): %lf\n", roadPitch);
			} else if (!strToken("THRESHOLD", buffer)) {
				sscanf(buffer, NON_NUMBER_STR"%lf", &threshold);
				fprintf(stdout, "INFO: Threshold (m/frame): %lf\n", threshold);
			} else if (!strToken("POINT", buffer)) {
				if (pTempIdx >= pCnt) {
					fflush(stdout);
					fprintf(stderr, "ERROR: More POINT then POINTS: %d\n", pTempIdx);
					fclose(fp);
					return EXIT_FAILURE;
				}
				ptemp_t* current = &pTemp[pTempIdx];
				sscanf(buffer, NON_NUMBER_STR"%u"NON_NUMBER_STR"%u"NON_NUMBER_STR"%u"NON_NUMBER_STR"%f"NON_NUMBER_STR"%f", &current->screenY, &current->screenX1, &current->screenX2, &current->roadX1, &current->roadX2);
				fprintf(stdout, "INFO: Point %d: y@%u, x1@%u(%f), x2@%u(%f)\n", pTempIdx, current->screenY, current->screenX1, current->roadX1, current->screenX2, current->roadX2);
				pTempIdx++;
			} else {
				fflush(stdout);
				fprintf(stderr, "ERROR: Cannot process info: %s\n", buffer);
				fclose(fp);
				return EXIT_FAILURE;
			}
		}
		fflush(stdout);
		fclose(fp);

		if (pTempIdx != pCnt) {
			fputs("ERROR: POINT and POINTS mismatched\n", stderr);
			return EXIT_FAILURE;
		}
		for (unsigned int i = 0; i < pCnt - 1; i++) {
			if (pTemp[i].screenY >= pTemp[i+1].screenY) {
				fputs("ERROR: POINT in bad order, should be from screen top (low y-coord) to bottom (high y-coord)\n", stderr);
				return EXIT_FAILURE;
			}
		}
		if (installHeight <= 0.0 || installPitch < -180.0 || installPitch > 180.0) {
			fprintf(stderr, "ERROR: Bad config: Install height is: %lf, need: >0.0; Install pitch is: %lf, need: range[-180.0,180.0]\n", installHeight, installPitch);
			return EXIT_FAILURE;
		}
		if ( width <= 320 || height <= 240 || width & (typeof(width))0b11 || height &(typeof(height))0b11 ) {
			fprintf(stderr, "ERROR: Bad config: Width is: %u, need: >320 and multiply of 4; Height is: %u, need: >240 and multiply of 4\n", width, height);
			return EXIT_FAILURE;
		}
		if (fovHorizontal <= 0.0 || fovVertical <= 0.0) {
			fprintf(stderr, "ERROR: Bad config: FOV horizontal is: %lf, need: >0.0; FOV vertical is: %lf, need: >0.0\n", fovHorizontal, fovVertical);
			return EXIT_FAILURE;
		}
		if (roadPitch < -180.0 || roadPitch > 180.0) {
			fprintf(stderr, "ERROR: Bad config: Road pitch is: %lf, need: range[-180.0,180.0]\n", roadPitch);
			return EXIT_FAILURE;
		}
		if ( fabs( (width/fovHorizontal) / (height/fovVertical) - 1 ) > 0.05 )
			fprintf(stdout, "WARNING: Horizontal PPD (pixel/degree) is %lf, but vertical PPD is %lf\n", width/fovHorizontal, height/fovVertical);
	}
	point_t p[pCnt][2];
	for (int i = 0; i < pCnt; i++) {
		p[i][0] = (point_t){.screenX = pTemp[i].screenX1, .screenY = pTemp[i].screenY, .roadX = pTemp[i].roadX1};
		p[i][1] = (point_t){.screenX = pTemp[i].screenX2, .screenY = pTemp[i].screenY, .roadX = pTemp[i].roadX2};
	}
	
	/* Create file & Prepare file content */
	FILE* fp = fopen(argv[2], "wb");
	if (!fp) {
		fprintf(stderr, "ERROR: Cannot open file %s (errno=%d)\n", argv[2], errno);
		return EXIT_FAILURE;
	}

	header_t header = {
		.width = width, .height = height,
		.searchThreshold = threshold,
		.orthoPixelWidth = 0.0, //TBD
		.searchDistanceXOrtho = 0, //TBD
	};
	data1_t (*t1)[width] = malloc(sizeof(data1_t) * height * width);
	data2_t (*t2)[width] = malloc(sizeof(data2_t) * height * width);
	if (!t1 || !t2) {
		fprintf(stderr, "ERROR: Cannot allocate memory for table storage (errno=%d)\n", errno);
		fclose(fp);
		if (t1)
			free(t1);
		if (t2)
			free(t2);
		return EXIT_FAILURE;
	}

	/* Find road data */ {
		fputs("INFO: Finding road geographic data\n", stdout);
		fflush(stdout);

		/* Perspective */ {
			for (unsigned int y = 0; y < height; y++) {
				double lookPitch = installPitch + 0.5 * fovVertical - y / (height - 1.0) * fovVertical;
				double dHeight = roadPitch - lookPitch, dY = 90 + lookPitch, dRoad = 90 - roadPitch;
				double py = (installHeight / dsin(dHeight)) * dsin(dY); //Road y-coord

				double leftSlopeScreen, leftOffsetScreen, leftGainScreen, rightSlopeScreen, rightOffsetScreen, rightGainScreen;
				double leftSlopeRoad, leftOffsetRoad, leftGainRoad, rightSlopeRoad, rightOffsetRoad, rightGainRoad;
				if (y < p[0][0].screenY) {
					findSlopeEq(p[0][0], p[1][0], &leftSlopeScreen, &leftOffsetScreen, &leftGainScreen, &leftSlopeRoad, &leftOffsetRoad, &leftGainRoad);
					findSlopeEq(p[0][1], p[1][1], &rightSlopeScreen, &rightOffsetScreen, &rightGainScreen, &rightSlopeRoad, &rightOffsetRoad, &rightGainRoad);
				} else if (y > p[pCnt-1][0].screenY) {
					findSlopeEq(p[pCnt-2][0], p[pCnt-1][0], &leftSlopeScreen, &leftOffsetScreen, &leftGainScreen, &leftSlopeRoad, &leftOffsetRoad, &leftGainRoad);
					findSlopeEq(p[pCnt-2][1], p[pCnt-1][1], &rightSlopeScreen, &rightOffsetScreen, &rightGainScreen, &rightSlopeRoad, &rightOffsetRoad, &rightGainRoad);
				} else {
					for (unsigned int i = 0; i < pCnt - 1; i++) {
						if (isBetween(y, p[i][0].screenY, p[i+1][0].screenY) >= 0) {
							findSlopeEq(p[i][0], p[i+1][0], &leftSlopeScreen, &leftOffsetScreen, &leftGainScreen, &leftSlopeRoad, &leftOffsetRoad, &leftGainRoad);
							findSlopeEq(p[i][1], p[i+1][1], &rightSlopeScreen, &rightOffsetScreen, &rightGainScreen, &rightSlopeRoad, &rightOffsetRoad, &rightGainRoad);
							break;
						}
					}
				}

//				printf("Y %u - lSlope %lf, lOffset %lf, leftGain %lf, rSlope %lf, rOffset %lf, rGain %lf\n", y, leftSlopeScreen, leftOffsetScreen, leftGainScreen, rightSlopeScreen, rightOffsetScreen, rightGainScreen);
				
				double leftMarginScreen = (y + leftOffsetScreen) * leftSlopeScreen + leftGainScreen;
				double rightMarginScreen = (y + rightOffsetScreen) * rightSlopeScreen + rightGainScreen;
				double leftMarginRoad = (y + leftOffsetRoad) * leftSlopeRoad + leftGainRoad;
				double rightMarginRoad = (y + rightOffsetRoad) * rightSlopeRoad + rightGainRoad;
				double xStep = (rightMarginRoad - leftMarginRoad) / (rightMarginScreen - leftMarginScreen);

//				printf("Y %u (%.3lf) - Left at %u (%.3lf), right at %u (%.3lf), step %lf\n", y, py, (unsigned int)leftMarginScreen, leftMarginRoad, (unsigned int)rightMarginScreen, rightMarginRoad, xStep);

				double y0RoadX = leftMarginRoad - leftMarginScreen * xStep;
				for (unsigned int x = 0; x < width; x++) {
					t1[y][x].px = y0RoadX + x * xStep;
					t1[y][x].py = py;
				}
			}
		}

		/* Orthographic */ {
			double baselineLeft = t1[height-1][0].px, baselineRight = t1[height-1][width-1].px;
			double xStep = (baselineRight - baselineLeft) / (width - 1.0);
			for (unsigned int y = 0; y < height; y++) {
				for (unsigned int x = 0; x < width; x++) {
					t1[y][x].ox = baselineLeft + xStep * x;
					t1[y][x].oy = t1[y][x].py; //Y-axis in orthographic view is same as in perspective view (we project in x-axis only)
				}
			}

			header.orthoPixelWidth = xStep;
		}
	}

	/* Calculate search distance */ {
		fputs("INFO: Calculating searching distance\n", stdout);
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
		fputs("INFO: Finding lookup table\n", stdout);
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
		fputs("INFO: Writing to file\n", stdout);
		fflush(stdout);
		fwrite(&header, sizeof(header), 1, fp);
		fwrite(t1, sizeof(t1[0][0]), height * width, fp);
		fwrite(t2, sizeof(t2[0][0]), height * width, fp);
		fwrite(&pCnt, sizeof(pCnt), 1, fp);
		fwrite(p, sizeof(point_t), pCnt * 2, fp);

		fputs("\n\n== Metadata: ===================================================================\n", fp);
		fprintf(fp, "Camera height: %lf m, pitch: %lf deg\n", installHeight, installPitch);
		fprintf(fp, "Camera resolution: %u x %u p, FOV: %lf x %lf deg\n", width, height, fovHorizontal, fovVertical);
		fprintf(fp, "Road picth: %lf deg\n", roadPitch);
		fprintf(fp, "Threshold: %lf m/frame\n", threshold);
		for (size_t i = 0; i < pCnt; i++) {
			fprintf(fp, "point %zu - Left: x = %u(%f), y = %u(%f)\n", i, p[i][0].screenX, p[i][0].roadX, p[i][0].screenY, p[i][0].roadY);
			fprintf(fp, "point %zu - Left: x = %u(%f), y = %u(%f)\n", i, p[i][1].screenX, p[i][1].roadX, p[i][1].screenY, p[i][1].roadY);
		}
	}

	fclose(fp);
	fprintf(stdout, "INFO: Roadmap file '%s' generated\n", argv[2]);
	fflush(stdout);
	return EXIT_SUCCESS;
}