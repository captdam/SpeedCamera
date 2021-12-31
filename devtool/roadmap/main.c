/** This program is used to generate road map file. 
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
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
 * See readme for detail 
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
	float roadX1, roadX2, roadY;
	unsigned int screenX1, screenX2, screenY;
} ptemp_t;

#define DELI "%*[^0-9.-]" //Delimiter
#define err(format, ...) (fprintf(stderr, "ERROR: "format __VA_OPT__(,) __VA_ARGS__))
#define info(format, ...) fprintf(stdout, "INFO: "format __VA_OPT__(,) __VA_ARGS__)

//Math in degree
#define d2r(deg) ((deg) * M_PI / 180.0)
#define r2d(rad) ((rad) * 180.0 / M_PI)
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

void polyFit2(double x1, double x2, double y1, double y2, double* cof) {
	cof[1] = (y2 - y1) / (x2 - x1);
	cof[0] = y1 - x1 * cof[1];
}

int readline(const char* src, const char* token, const unsigned int argc, const char* format, ...) {
	if (strncmp(token, src, strlen(token)))
		return 0;
	
	int result = 1; //Success
	va_list args;
	va_start(args, format);
	if (vsscanf(src, format, args) != argc) //If mismatch
		result = -1; //Change to fail
	va_end(args);
	
	return result;
}

int main(int argc, char* argv[]) {
	if (argc != 3) {
		fputs("ERROR: Bad arg: Use 'this info.txt output.data\n", stderr);
		fputs("\tinfo.txt is an ASCII file contains human-readable road info", stderr);
		fputs("\toutput.data is a binary file used to tell the program road info", stderr);
		return EXIT_FAILURE;
	}

	unsigned int width = 0, height = 0;
	double threshold = 0.0;
	point_t(* p)[2] = NULL;
	unsigned int pCnt = 0;
	header_t header;
	data1_t* t1 = NULL;
	data2_t* t2 = NULL;
	FILE* fp = NULL;
	
	/* Get config from file */ {
		fp = fopen(argv[1], "r");
		if (!fp) {
			err("Cannot open file: %s (errno=%d)\n", argv[1], errno);
			goto label_read_error;
		}
		char buffer[500];
		int res;
		unsigned int screenX1, screenX2, screenY;
		float roadX1, roadX2, roadY;
		unsigned int pIdx = 0;

		while (fgets(buffer, sizeof(buffer), fp)) {
			
			if (res = readline(buffer, "POINTS", 1, DELI"%u", &pCnt)) {
				if (res == -1) {
					err("Cannot read POINTS: bad format, require '...uint'\n");
					goto label_read_error;
				}
				if (p) {
					err("Duplicated POINTS\n");
					goto label_read_error;
				}
				if (pCnt < 2) {
					err("Not enough POINTS, minimum 2\n");
					goto label_read_error;
				}
				p = malloc(sizeof(point_t) * pCnt * 2);
				if (!p) {
					err("Cannot allocate memory for points storage when reading info file (errno=%d)\n", errno);
					goto label_read_error;
				}
				info("POINTS: %u pairs\n", pCnt);
			
			} else if (res = readline(buffer, "WIDTH", 1, DELI"%u", &width)) {
				if (res == -1) {
					err("Cannot read WIDTH: bad format, require '...uint'\n");
					goto label_read_error;
				}
				if ( width < 320 || width & (typeof(width))0b11 ) {
					err("WIDTH - must be greater than or equal to 320 and be multiply of 4\n");
					goto label_read_error;
				}
				info("Width (px): %u\n", width);
			
			} else if (res = readline(buffer, "HEIGHT", 1, DELI"%u", &height)) {
				if (res == -1) {
					err("Cannot read HEIGHT: bad format, require '...uint'\n");
					goto label_read_error;
				}
				if ( height < 240 || height & (typeof(height))0b11 ) {
					err("HEIGHT - must be greater than or equal to 240 and be multiply of 4\n");
					goto label_read_error;
				}
				info("Height (px): %u\n", height);
			
			} else if (res = readline(buffer, "THRESHOLD", 1, DELI"%lf", &threshold)) {
				if (res == -1) {
					err("Cannot read THRESHOLD: bad format, require '...double'\n");
					goto label_read_error;
				}
				info("Threshold (m/frame): %.3lf\n", threshold);
			
			} else if (res = readline(buffer, "POINT", 6, DELI"%u"DELI"%u"DELI"%u"DELI"%f"DELI"%f"DELI"%f", &screenY, &screenX1, &screenX2, &roadY, &roadX1, &roadX2)) {
				if (res == -1) {
					err("Cannot read POINT: bad format, require '...uint...uint...uint...float...float...float'\n");
					goto label_read_error;
				}
				if (pIdx >= pCnt) {
					err("More POINT than POINTS (Count = %u)\n", pCnt);
					goto label_read_error;
				}
				if (pIdx) {
					if (screenY <= p[pIdx-1][0].screenY || roadY >= p[pIdx-1][0].roadY) {
						err("POINT - Screen Y should be in increasing order (%u -> %u), Road Y should be in decreasing order (%.2f -> %.2f)\n", p[pIdx-2][0].screenY, screenY, p[pIdx-2][0].roadY, roadY);
						goto label_read_error;
					}
					if (screenX2 <= screenX1 || roadX2 <= roadX1) {
						err("POINT - X2 (%u, %.2f) should have greater Screen and Road value than X1 (%u, %.2f)\n", screenX2, roadX2, screenX1, roadX1);
						goto label_read_error;
					}
				}
				p[pIdx][0] = (point_t){.screenX = screenX1, .screenY = screenY, .roadX = roadX1, .roadY = roadY};
				p[pIdx][1] = (point_t){.screenX = screenX2, .screenY = screenY, .roadX = roadX2, .roadY = roadY};
				info("POINT: PL(%u,%u)@(%.2f,%.2f)\n", p[pIdx][0].screenX, p[pIdx][0].screenY, p[pIdx][0].roadX, p[pIdx][0].roadY);
				info("POINT: PR(%u,%u)@(%.2f,%.2f)\n", p[pIdx][1].screenX, p[pIdx][1].screenY, p[pIdx][1].roadX, p[pIdx][1].roadY);
				pIdx++;
			
			} else {
				err("Unknown info: %s", buffer);
				goto label_read_error;
			}
		}
		fclose(fp);

		if (!width) {
			err("Missing config: WIDTH\n");
			goto label_read_error;
		}
		if (!height) {
			err("Missing config: HEIGHT\n");
			goto label_read_error;
		}
		if (threshold == 0.0) {
			err("Missing config: THRESHOLD\n");
			goto label_read_error;
		}

		goto label_read_success;
label_read_error:;
		if (fp) {
			fclose(fp);
			fp = NULL;
		}
		free(p);
		return EXIT_FAILURE;
label_read_success:;
	}
	
	/* Create file & Prepare file content */ {
		fp = fopen(argv[2], "wb");
		if (!fp) {
			err("Cannot open file %s (errno=%d)\n", argv[2], errno);
			goto label_prepare_error;
		}

		header = (typeof(header)){
			.width = width, .height = height,
			.searchThreshold = threshold,
			.orthoPixelWidth = 0.0, //TBD
			.searchDistanceXOrtho = 0, //TBD
		};
		t1 = malloc(sizeof(data1_t) * height * width);
		t2 = malloc(sizeof(data2_t) * height * width);
		if (!t1 || !t2) {
			err("Cannot allocate memory for table storage (errno=%d)\n", errno);
			goto label_prepare_error;
		}

		goto label_prepare_success;
label_prepare_error:;
		if (fp) {
			fclose(fp);
			fp = NULL;
		}
		free(t1);
		free(t2);
		return EXIT_FAILURE;
label_prepare_success:;
	}

	/* Find road data */ {
		info("Finding road geographic data\n");
		fflush(stdout);

		/* Perspective */ {
			for (unsigned int y = 0; y < height; y++) {
				point_t pTL, pTR, pBL, pBR;
				double cofScreenLeft[2], cofScreenRight[2], cofRoadLeft[2], cofRoadRight[2], cofRoadDistance[2];
				if (y < p[0][0].screenY) {
					pTL = p[0][0];
					pTR = p[0][1];
					pBL = p[1][0];
					pBR = p[1][1];
				} else if (y > p[pCnt-1][0].screenY) {
					pTL = p[pCnt-2][0];
					pTR = p[pCnt-2][1];
					pBL = p[pCnt-1][0];
					pBR = p[pCnt-1][1];
				} else {
					for (unsigned int i = 0; i < pCnt - 1; i++) {
						if (isBetween(y, p[i][0].screenY, p[i+1][0].screenY) >= 0) {
							pTL = p[i][0];
							pTR = p[i][1];
							pBL = p[i+1][0];
							pBR = p[i+1][1];
							break;
						}
					}
				}
				polyFit2(pTL.screenY, pBL.screenY, pTL.screenX, pBL.screenX, cofScreenLeft); //All: Give screen y-coord, return other data
				polyFit2(pTR.screenY, pBR.screenY, pTR.screenX, pBR.screenX, cofScreenRight);
				polyFit2(pTL.screenY, pBL.screenY, pTL.roadX, pBL.roadX, cofRoadLeft);
				polyFit2(pTR.screenY, pBR.screenY, pTR.roadX, pBR.roadX, cofRoadRight);
				polyFit2(pTL.screenY, pBL.screenY, pTL.roadY, pBL.roadY, cofRoadDistance);

				double screenLeft = y * cofScreenLeft[1] + cofScreenLeft[0], roadLeft = y * cofRoadLeft[1] + cofRoadLeft[0];
				double screenRight = y * cofScreenRight[1] + cofScreenRight[0], roadRight = y * cofRoadRight[1] + cofRoadRight[0];
				double xStep = (roadRight - roadLeft) / (screenRight - screenLeft);
				double px0 = roadLeft - xStep * screenLeft; //Road px when y is 0
				double py = y * cofRoadDistance[1] + cofRoadDistance[0];
//				printf("Y %u (%.3lf) - Left at %u (%.3lf), right at %u (%.3lf), step %lf\n", y, py, (unsigned int)screenLeft, roadLeft, (unsigned int)screenRight, roadRight, xStep);

				for (unsigned int x = 0; x < width; x++) {
					unsigned int currentIdx = y * width + x;
					t1[currentIdx].px = px0 + x * xStep;
					t1[currentIdx].py = py;
				}
			}
		}

		/* Orthographic */ {
			double baselineLeft = t1[ (height-1) * width + 0 ].px, baselineRight = t1[ (height-1) * width + (width-1) ].px;
			double xStep = (baselineRight - baselineLeft) / (width - 1.0);
			for (unsigned int y = 0; y < height; y++) {
				for (unsigned int x = 0; x < width; x++) {
					unsigned int currentIdx = y * width + x;
					t1[currentIdx].ox = baselineLeft + xStep * x;
					t1[currentIdx].oy = t1[currentIdx].py; //Y-axis in orthographic view is same as in perspective view (we project in x-axis only)
				}
			}

			header.orthoPixelWidth = xStep;
		}
	}

	/* Calculate search distance */ {
		info("Calculating searching distance\n");
		fflush(stdout);
		for (unsigned int y = 0; y < height; y++) {
			for (unsigned int x = 0; x < width; x++) {
				unsigned int currentIdx = y * width + x;
				point_t persp = {.roadX = t1[currentIdx].px, .roadY = t1[currentIdx].py};
				unsigned int left = 0, right = 0, up = 0, down = 0;
				while (x - left > 0 && fabs( t1[currentIdx-left ].px - persp.roadX ) <= threshold)
					left++;
				while (x + right < width - 1 && fabs( t1[currentIdx+right].px - persp.roadX ) <= threshold)
					right++;
				while (y - up > 0 && fabs( t1[currentIdx - up * width].py - persp.roadY ) <= threshold)
					up++;
				while (y + down < height - 1 && fabs( t1[currentIdx + down * width].py - persp.roadY ) <= threshold)
					down++;

				t2[currentIdx].searchDistanceXPersp = left > right ? left : right;
				t2[currentIdx].searchDistanceY = up > down ? up : down;
			}
		}

		header.searchDistanceXOrtho = threshold / header.orthoPixelWidth;
	}

	/* Find lookup table */ {
		info("Finding lookup table\n");
		fflush(stdout);
		for (unsigned int y = 0; y < height; y++) {
			for (unsigned int x = 0; x < width; x++) {
				unsigned int currentIdx = y * width + x;

				/* Perspective to Orthographic */ {
					float road = t1[currentIdx].ox;
					if (road <= t1[ y * width + 0 ].px) {
						t2[currentIdx].lookupXp2o = 0;
					} else if (road >= t1[ y * width + (width-1) ].px) {
						t2[currentIdx].lookupXp2o = width - 1;
					} else {
						for (unsigned int i = 0; i < width - 1; i++) {
							float left = t1[ y * width + i ].px, right = t1[ y * width + (i+1) ].px;
							int check = isBetween(road, left, right);
							if (check != -1) {
								t2[currentIdx].lookupXp2o = i + check;
								break;
							}
						}
					}
				}

				/* Orthographic to Perspective */ {
					float road = t1[currentIdx].px;
					if (road <= t1[ y * width + 0 ].ox) {
						t2[currentIdx].lookupXo2p = 0;
					} else if (road >= t1[ y * width + (width-1) ].ox) {
						t2[currentIdx].lookupXo2p = width - 1;
					} else {
						for (unsigned int i = 0; i < width - 1; i++) {
							float left = t1[ y * width + i ].ox, right = t1[ y * width + (i+1) ].ox;
							int check = isBetween(road, left, right);
							if (check != -1) {
								t2[currentIdx].lookupXo2p = i + check;
								break;
							}
						}
					}
				}
			}
		}
	}

	/* Write to file */ {
		info("Writing to file\n");
		fflush(stdout);
		fwrite(&header, sizeof(header), 1, fp);
		fwrite(t1, sizeof(data1_t), height * width, fp);
		fwrite(t2, sizeof(data2_t), height * width, fp);
		fwrite(&pCnt, sizeof(pCnt), 1, fp);
		fwrite(p, sizeof(point_t), pCnt * 2, fp);

		fputs("\n\n== Metadata: ===================================================================\n", fp);
		fprintf(fp, "Camera resolution: %upx x %upx (%usqpx)\n", width, height, width * height);
		fprintf(fp, "Threshold: %lf m/frame\n", threshold);
		for (size_t i = 0; i < pCnt; i++) {
			fprintf(fp, "point %zu - L: x = %u(%f), y = %u(%f)\n", i, p[i][0].screenX, p[i][0].roadX, p[i][0].screenY, p[i][0].roadY);
			fprintf(fp, "point %zu - R: x = %u(%f), y = %u(%f)\n", i, p[i][1].screenX, p[i][1].roadX, p[i][1].screenY, p[i][1].roadY);
		}
	}

	free(t1);
	free(t2);
	free(p);
	fclose(fp);
	info("Roadmap file '%s' generated\n", argv[2]);
	fflush(stdout);
	return EXIT_SUCCESS;
}