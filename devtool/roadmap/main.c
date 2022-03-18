/** This program is used to generate roadmap file. 
 * See the readme.md for detail. 
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <inttypes.h>
#include <errno.h>
#include <math.h>
#include <string.h>

/** Output file fomat:
 * Section I:	File header. 
 * Section II:	Table 1: Road-domain geographic data in prospective and orthographic views. 
 * Section III:	Table 2: Search distance, prospective and orthographic projection map. 
 * Section IV:	Number of points pair, focus region points. 
 * Section V:	Meta data: ASCII meta data, for reference, ignored by program. 
 */

// Section I: File header
typedef struct FileHeader {
	uint32_t width, height; //Frame size in pixel, also used to calculate the size of this file
} header_t;

// Section II: Table 1: Road-domain geographic data in prospective and orthographic views
typedef struct FileDataTable1 {
	float px, py; //Road-domain geographic data in prospective view
	float ox, oy; //Road-domain geographic data in orthographic view
} data1_t;

// Section III: Table 2: Search distance, prospective and orthographic projection map
typedef struct FileDataTable2 {
	uint32_t searchLimitUp, searchLimitDown; //Up/down search limit y-coord
	uint32_t lookupXp2o, lookupXo2p; //Projection lookup table. Y-crood in orthographic and perspective views are same
} data2_t;

// Section IV: Number of points pair, focus region points
typedef uint32_t pCnt_t;
typedef struct Point_t {
	float rx, ry; //Road-domain location (m)
	float sx, sy; //Screen domain position (0.0 for left and top, 1.0 for right and bottom)
} point_t;

#define DELI "%*[^0-9.-]" //Delimiter
#define err(format, ...) {fprintf(stderr, "ERROR: "format __VA_OPT__(,) __VA_ARGS__);}
#define info(format, ...) {fprintf(stderr, "INFO:  "format __VA_OPT__(,) __VA_ARGS__);}

//Return 0 if X is closer to A, 1 if X is closer to B, -1 if X is smaller than A or greater then B
int isBetween(float x, float a, float b) {
	if (x < a || x > b)
		return -1;
	return ((x - a) <= (b - x)) ? 0 : 1;
}

//Find the first-order equation to describe the line connect two points: y = x * cof[1] + cof[0]
void polyFit2(float x1, float x2, float y1, float y2, float* cof) {
	cof[1] = (y2 - y1) / (x2 - x1);
	cof[0] = y1 - x1 * cof[1];
}

//Read a line of string, return 0 if token not found, 1 if token found and argument count matched, -1 if argument count doesn't match
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

header_t header;
data1_t* t1;
data2_t* t2;
pCnt_t pCnt;
point_t(* p)[2];

void new() __attribute__ ((constructor));
void new() {
	header.width = 0;
	header.height = 0;
	t1 = NULL;
	t2 = NULL;
	pCnt = 0;
	p = NULL;
}

void destroy() __attribute__ ((destructor));
void destroy() {
	free(t1);
	free(t2);
	free(p);
}

int main(int argc, char* argv[]) {
	if (argc != 3) {
		err("ERROR: Bad arg: Use this width height < info.txt > output.map\n");
		err("\twidth and height is the size of frame in px\n");
		err("\tinfo.txt (stdin): an ASCII file contains human-readable road\n");
		err("\toutput.map (stdout): a binary roadmap file\n");
		return EXIT_FAILURE;
	}
	
	/* Get size */ {
		info("Get frame size and allocate buffer for tables\n");
		header.width = atoi(argv[1]);
		header.height = atoi(argv[2]);
		if ( header.width < 320 || header.width & (typeof(header.width))0b11 ) {
			err("SIZE - width must be greater than or equal to 320 and be multiply of 4\n");
			return EXIT_FAILURE;
		}
		if ( header.height < 240 || header.height & (typeof(header.height))0b11 ) {
			err("SIZE - height must be greater than or equal to 240 and be multiply of 4\n");
			return EXIT_FAILURE;
		}

		t1 = malloc(sizeof(data1_t) * header.height * header.width);
		t2 = malloc(sizeof(data2_t) * header.height * header.width);
		if (!t1 || !t2) {
			err("Cannot allocate memory for table storage (errno=%d)\n", errno);
			return EXIT_FAILURE;
		}
	}

	/* Get config from stdin */ {
		info("Process road points\n");
		char buffer[500];
		int res; //Result of readline() 0 = token not match, 1 = ok, -1 = argument count mismatch
		float sy, sxl, sxr, ry, rxl, rxr; //Screen/road point y/x-left/x-right coord

		while (fgets(buffer, sizeof(buffer), stdin)) {
			if (res = readline(buffer, "POINT", 6, DELI"%f"DELI"%f"DELI"%f"DELI"%f"DELI"%f"DELI"%f", &sy, &sxl, &sxr, &ry, &rxl, &rxr)) {
				if (res == -1) {
					err("Cannot read POINT: bad format, require '...float...float...float...float...float...float'\n");
					return EXIT_FAILURE;
				}
				if (!( p = realloc(p, (pCnt+1) * sizeof(point_t) * 2) )) {
					err("Cannot allocate memory for points storage when reading info file (errno=%d)\n", errno);
					return EXIT_FAILURE;
				}
				if (pCnt) { //This check does not apply on first road point pair
					if (sy <= p[pCnt-1][0].sy) {
						err("POINT %u - Screen Y should be in top-to-bottom order, current y-coord (%.4f) should be greater than last y-coord (%.4f)\n", pCnt, sy, p[pCnt-1][0].sy);
						return EXIT_FAILURE;
					}
					if (ry >= p[pCnt-1][0].ry) {
						err("POINT %u - Road point at bottom position should be closer, current y-coord (%.4f) should be lower than last y-coord (%.4f)\n", pCnt, ry, p[pCnt-2][0].ry);
						return EXIT_FAILURE;
					}
					if (sxr <= sxl) {
						err("POINT %u - Right point (%.4f) should have greater x-coord than left point (%.4f)\n", pCnt, sxr, sxl);
						return EXIT_FAILURE;
					}
					if (rxr <= rxl) {
						err("POINT %u - Right point (%.4f) should have greater x-coord than left point (%.4f)\n", pCnt, rxr, rxl);
						return EXIT_FAILURE;
					}
				}
				p[pCnt][0] = (point_t){.sx = sxl, .sy = sy, .rx = rxl, .ry = ry};
				p[pCnt][1] = (point_t){.sx = sxr, .sy = sy, .rx = rxr, .ry = ry};
				unsigned int usxl = p[pCnt][0].sx * header.width, usxr = p[pCnt][1].sx * header.width, usy = p[pCnt][0].sy * header.height;
				info("POINT: PL pos (%.4f,%.4f) <%upx,%upx> @ loc (%.4f,%.4f)\n", p[pCnt][0].sx, p[pCnt][0].sy, usxl, usy, p[pCnt][0].rx, p[pCnt][0].ry);
				info("POINT: PR pos (%.4f,%.4f) <%upx,%upx> @ loc (%.4f,%.4f)\n", p[pCnt][1].sx, p[pCnt][1].sy, usxr, usy, p[pCnt][1].rx, p[pCnt][1].ry);
				pCnt++;
			} else {
				err("Unknown info: %s", buffer);
				return EXIT_FAILURE;
			}
		}
	}

	/* Find road data */ {

		/* Perspective */ {
			info("Generate perspective view roadmap\n");
			float csl[2], csr[2], crl[2], crr[2], cry[2], crx[2]; //Coefficient screen/road left-x/right-x of focus region mesh, road y relative to screen y-coord, coefficient road x to screen x
			unsigned int currentPointIdx = -1; //0xFFFFFFFF
			for (unsigned int y = 0; y < header.height; y++) {
				float yNorm = (float)y / header.height; //Normalized y-coord
				unsigned int requestPointIndex;
				if (yNorm < p[0][0].sy) {
					requestPointIndex = 0;
				} else if (yNorm > p[pCnt-1][0].sy) {
					requestPointIndex = pCnt - 2;
				} else {
					for (unsigned int i = 0; i < pCnt - 2; i++) {
						if (isBetween(yNorm, p[i][0].sy, p[i+1][0].sy) >= 0) {
							requestPointIndex = i;
							break;
						}
					}
				}

				if (currentPointIdx != requestPointIndex) {
					unsigned int up = requestPointIndex, down = requestPointIndex + 1;
	//				info("Y pos %u/%.4f loc %.4f): Update: point_up: idx%u %.4f; point_down: idx%u %.4f\n", y, yNorm, (float)y / size.height, requestPointIndex, p[up][0].sy, requestPointIndex+1, p[down][0].sy);
					point_t pTL = p[up][0], pTR = p[up][1];
					point_t pBL = p[down][0], pBR = p[down][1];
					polyFit2(pTL.sy, pBL.sy, pTL.sx, pBL.sx, csl); //Equation: left point screen x-coord relative to screen y-coord (left of the focus region mesh)
					polyFit2(pTR.sy, pBR.sy, pTR.sx, pBR.sx, csr); //Right point screen x-coord relative to screen y-coord
					polyFit2(pTL.sy, pBL.sy, pTL.rx, pBL.rx, crl); //Left point road x-coord relative to screen y-coord
					polyFit2(pTR.sy, pBR.sy, pTR.rx, pBR.rx, crr); //Right point road x-coord relative to screen y-coord
					polyFit2(pTL.sy, pBL.sy, pTL.ry, pBL.ry, cry); //Road y-ccord relative to screen y-coord
					currentPointIdx = requestPointIndex;
				}

				float slNorm = yNorm * csl[1] + csl[0], rl = yNorm * crl[1] + crl[0];
				float srNorm = yNorm * csr[1] + csr[0], rr = yNorm * crr[1] + crr[0];
				float ry = yNorm * cry[1] + cry[0];
				unsigned int sl = slNorm * header.width, sr = srNorm * header.width;
				if (y % 16 == 0) info("Y pos %u/%.4f loc %.4f, left at pos %u/%.4f loc %.4f, right pos %u/%.4f loc %.4f\n", y, yNorm, ry, sl, slNorm, rl, sr, srNorm, rr);
				polyFit2(slNorm, srNorm, rl, rr, crx); //Road x-coord relative to screen x-coord
				for (unsigned int x = 0; x < header.width; x++) {
					float xNorm = (float)x / header.width; //Normalized x-coord
					unsigned int i = y * header.width + x;
					t1[i].px = crx[1] * xNorm + crx[0];
					t1[i].py = ry;
				}
			}
		}

		/* Orthographic */ {
			info("Generate orthographic view roadmap\n");
			float cx[2];
			float baselineLeft = t1[ (header.height-1) * header.width + 0 ].px, baselineRight = t1[ header.height * header.width - 1 ].px;
			polyFit2(0, header.width - 1, baselineLeft, baselineRight, cx);
			for (unsigned int y = 0; y < header.height; y++) {
				for (unsigned int x = 0; x < header.width; x++) {
					unsigned int i = y * header.width + x;
					t1[i].ox = x * cx[1] + cx[0];
					t1[i].oy = t1[i].py; //Y-axis in orthographic view is same as in perspective view (we project in x-axis only)
				}
			}
		}
	}

	/* Calculate search distance */ {
		info("Calculate search distance\n");
		/* Notes:
		 * 1 - Use limit instead of distance:
		 * Some GL driver (eg RPI) support texelFetchOffset() with const offset only; therefore GLSL code 
		 * "for (ivec2 OFFSET; ...; offset += DISTANCE) { xxx = texelFetchOffset(sampler, base, lod, OFFSET); }" 
		 * will not work. Then we have to use GLSL code
		 * "for (ivec2 IDX; idx < LIMIT; idx++) { xxx = texelFetch(sampler, IDX, lod); }".
		 * 2 - Let the program decide the limit: 
		 * The search distance is the maximum distance that an object could reasonably travel between two frames. 
		 * Since the input video FPS is not known, it doesn't make sense defining the limit now. However, what we
		 * know now is the up and down edge of the focus region. 
		 */
		unsigned int limitUp = p[0][0].sy * header.height, limitDown = p[pCnt-1][0].sy * header.height;
		for (unsigned int y = 0; y < header.height; y++) {
			data2_t* ptr = t2 + y * header.width;
			if (y < limitUp || y > limitDown) {
				for (unsigned int x = header.width; x; x--) {
					ptr->searchLimitUp = y;
					ptr->searchLimitDown = y;
					ptr++;
				}
			} else {
				for (unsigned int x = header.width; x; x--) {
					ptr->searchLimitUp = limitUp;
					ptr->searchLimitDown = limitDown;
					ptr++;
				}
			}
		}
	}

	/* Find lookup table */ {
		info("Finding view transform lookup table\n");
		for (unsigned int y = 0; y < header.height; y++) {
			for (unsigned int x = 0; x < header.width; x++) {
				unsigned int left = y * header.width, right = left + header.width - 1;
				unsigned int current = left + x;

				/* Perspective to Orthographic */ {
					float road = t1[current].ox;
					if (road <= t1[left].px) {
						t2[current].lookupXp2o = 0;
					} else if (road >= t1[right].px) {
						t2[current].lookupXp2o = header.width - 1;
					} else {
						for (unsigned int i = 0; i < header.width - 2; i++) {
							int check = isBetween(road, t1[left+i].px, t1[left+i+1].px);
							if (check != -1) {
								t2[current].lookupXp2o = i + check;
								break;
							}
						}
					}
				}

				/* Orthographic to Perspective */ {
					float road = t1[current].px;
					if (road <= t1[left].ox) {
						t2[current].lookupXo2p = 0;
					} else if (road >= t1[right].ox) {
						t2[current].lookupXo2p = header.width - 1;
					} else {
						for (unsigned int i = 0; i < header.width - 2; i++) {
							int check = isBetween(road, t1[left+i].ox, t1[left+i+1].ox);
							if (check != -1) {
								t2[current].lookupXo2p = i + check;
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
		fwrite(&header, sizeof(header), 1, stdout);
		fwrite(t1, sizeof(data1_t), header.height * header.width, stdout);
		fwrite(t2, sizeof(data2_t), header.height * header.width, stdout);
		fwrite(&pCnt, sizeof(pCnt), 1, stdout);
		fwrite(p, sizeof(point_t), pCnt * 2, stdout);

		fputs("\n\n== Metadata: ===================================================================\n", stdout);
		fprintf(stdout, "Camera resolution: %upx x %upx (%usqpx)\n", header.width, header.height, header.width * header.height);
		for (size_t i = 0; i < pCnt; i++) {
			unsigned int usxl = p[i][0].sx * header.width, usxr = p[i][1].sx * header.width, usy = p[i][0].sy * header.height;
			fprintf(stdout, "POINT: PL pos (%.4f,%.4f) <%upx,%upx> @ loc (%.4f,%.4f)\n", p[i][0].sx, p[i][0].sy, usxl, usy, p[i][0].rx, p[i][0].ry);
			fprintf(stdout, "POINT: PR pos (%.4f,%.4f) <%upx,%upx> @ loc (%.4f,%.4f)\n", p[i][1].sx, p[i][1].sy, usxr, usy, p[i][1].rx, p[i][1].ry);
		}
	}

	info("Done!\n");
	return EXIT_SUCCESS;
}