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
 * Section II:	Table 1: Road-domain geographic data in perspective and orthographic views. 
 * Section III:	Table 2: Search distance, perspective and orthographic projection map. 
 * Section IV:	Focus region points. 
 * Section V:	Meta data: ASCII meta data, for reference, ignored by program. 
 */

// Section I: File header
typedef struct FileHeader {
	uint32_t width, height; //Frame size in pixel
	uint32_t pCnt; //Number of road points (perspective view)
	uint32_t fileSize; //Size of roadmap file without meta data, in byte
} header_t;

// Section II: Table 1: Road-domain geographic data in perspective and orthographic views
typedef struct FileDataTable1 {
	float px, py; //Road-domain geographic data in perspective view
	float ox; //Road-domain geographic data in orthographic view, y-coord of orthographic view is same as y-coord of perspective view
	float pw; //Perspective pixel width
} data1_t;

// Section III: Table 2: Search distance, perspective and orthographic projection map
typedef struct FileDataTable2 {
	float searchLimitUp, searchLimitDown; //Up/down search limit y-coord
	float lookupXp2o, lookupXo2p; //Projection lookup table. Y-crood in orthographic and perspective views are same
} data2_t;

// Section IV: Focus region points
typedef struct Point_t {
	float rx, ry; //Road-domain location (m)
	float sx, sy; //Screen domain position (NTC)
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
point_t(* p)[2];

void new() __attribute__ ((constructor));
void new() {
	header.width = 0;
	header.height = 0;
	header.pCnt = 0;
	header.fileSize = 0;
	t1 = NULL;
	t2 = NULL;
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

		if (header.width > 2048 || header.width < 320 || (unsigned int)header.width & (unsigned int)0b111) { //float16 interval > 0 if value > 2048 (abs)
			err("Width cannot be greater than 2048 or less than 320, and must be multiply of 8\n");
			return EXIT_FAILURE;
		}
		if (header.height > 2048 || header.height < 240 || (unsigned int)header.height & (unsigned int)0b111) {
			err("Height cannot be greater than 2048 or less than 240, and must be multiply of 8\n");
			return EXIT_FAILURE;
		}
		/* Note:
		Float16 use 10 bits frac.
		For absolute coord, when value > 2048, interval > 1.
		For NTC, interval = 2^-11, which is 1/2048 resolution.
		*/

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
		unsigned pPairCnt = 0;

		/* Road points - read for persp*/
		while (fgets(buffer, sizeof(buffer), stdin)) {
			if (res = readline(buffer, "POINT", 6, DELI"%f"DELI"%f"DELI"%f"DELI"%f"DELI"%f"DELI"%f", &sy, &sxl, &sxr, &ry, &rxl, &rxr)) {
				if (res == -1) {
					err("Cannot read POINT: bad format, require '...float...float...float...float...float...float'\n");
					return EXIT_FAILURE;
				}
				if (!( p = realloc(p, (pPairCnt+1) * sizeof(point_t) * 2) )) {
					err("Cannot allocate memory for points storage when reading info file (errno=%d)\n", errno);
					return EXIT_FAILURE;
				}

				point_t* current = p[pPairCnt];
				current[0] = (point_t){.sx = sxl, .sy = sy, .rx = rxl, .ry = ry};
				current[1] = (point_t){.sx = sxr, .sy = sy, .rx = rxr, .ry = ry};
				unsigned int sxl = current[0].sx * header.width, sxr = current[1].sx * header.width, sy = current[0].sy * header.height;
				info("POINT: PL pos (%.4f,%.4f) <%upx,%upx> @ loc (%.4f,%.4f)\n", current[0].sx, current[0].sy, sxl, sy, current[0].rx, current[0].ry);
				info("POINT: PR pos (%.4f,%.4f) <%upx,%upx> @ loc (%.4f,%.4f)\n", current[1].sx, current[1].sy, sxr, sy, current[1].rx, current[1].ry);

				if (pPairCnt) { //This check does not apply on first road point pair
					point_t* previous = p[pPairCnt - 1];
					if (current[0].sy <= previous[0].sy) {
						err("POINT %u - Screen y-coord should be in top-to-bottom order, current y-coord (%.4f) should be greater than last y-coord (%.4f)\n", pPairCnt, current[0].sy, previous[0].sy);
						return EXIT_FAILURE;
					}
					if (current[0].ry >= previous[0].ry) {
						err("POINT %u - Road point at bottom position should be closer, current y-coord (%.4f) should be lower than last y-coord (%.4f)\n",pPairCnt, current[0].ry, previous[0].ry);
						return EXIT_FAILURE;
					}
					if (current[1].sx <= current[0].sx) {
						err("POINT %u - Right point (%.4f) should have greater screen x-coord than left point (%.4f)\n", pPairCnt, current[1].sx, current[0].sx);
						return EXIT_FAILURE;
					}
					if (current[1].rx <= current[0].rx) {
						err("POINT %u - Right point (%.4f) should have greater road x-coord than left point (%.4f)\n", pPairCnt, current[1].rx, current[0].rx);
						return EXIT_FAILURE;
					}
				}

				pPairCnt++;
			} else {
				err("Unknown info: %s", buffer);
				return EXIT_FAILURE;
			}

			
		}

		/* Road points - gen for ortho */
		float sol = p[0][0].sx, sor = p[0][0].sx, sot = p[0][0].sy, sob = p[0][0].sy, rol = p[0][0].rx, ror = p[0][0].rx, rot = p[0][0].ry, rob = p[0][0].ry;
		for (unsigned int i = 0; i < pPairCnt; i++) {
			if (p[i][0].sx < sol) {
				sol = p[i][0].sx;
				rol = p[i][0].rx;
			}
			if (p[i][1].sx > sor) {
				sor = p[i][1].sx;
				ror = p[i][1].rx;
			}
			if (p[i][1].sy < sot) {
				sot = p[i][1].sy;
				rot = p[i][1].ry;
			}
			if (p[i][1].sy > sob) {
				sob = p[i][1].sy;
				rob = p[i][1].ry;
			}
		}
		if (!( p = realloc(p, (pPairCnt+2) * sizeof(point_t) * 2) )) {
			err("Cannot allocate memory for points storage when generting outer box (errno=%d)\n", errno);
			return EXIT_FAILURE;
		}
		p[pPairCnt+0][0] = (point_t){.sx = sol, .sy = sot, .rx = rol, .ry = rot};
		p[pPairCnt+0][1] = (point_t){.sx = sor, .sy = sot, .rx = ror, .ry = rot};
		p[pPairCnt+1][0] = (point_t){.sx = sol, .sy = sob, .rx = rol, .ry = rob};
		p[pPairCnt+1][1] = (point_t){.sx = sor, .sy = sob, .rx = ror, .ry = rob};
		info("POINT: TL pos (%.4f,%.4f) @ loc (%.4f,%.4f)\n", p[pPairCnt+0][0].sx, p[pPairCnt+0][0].sy, p[pPairCnt+0][0].rx, p[pPairCnt+0][0].ry);
		info("POINT: TR pos (%.4f,%.4f) @ loc (%.4f,%.4f)\n", p[pPairCnt+0][1].sx, p[pPairCnt+0][1].sy, p[pPairCnt+0][1].rx, p[pPairCnt+0][1].ry);
		info("POINT: BL pos (%.4f,%.4f) @ loc (%.4f,%.4f)\n", p[pPairCnt+1][0].sx, p[pPairCnt+1][0].sy, p[pPairCnt+1][0].rx, p[pPairCnt+1][0].ry);
		info("POINT: BR pos (%.4f,%.4f) @ loc (%.4f,%.4f)\n", p[pPairCnt+1][1].sx, p[pPairCnt+1][1].sy, p[pPairCnt+1][1].rx, p[pPairCnt+1][1].ry);
		pPairCnt += 2;

		header.pCnt = pPairCnt * 2;
		header.fileSize = sizeof(header_t) + header.width * header.height * (sizeof(data1_t) + sizeof(data2_t)) + header.pCnt * sizeof(point_t);
	}

	/* Find road data */ {

		/* Perspective */ {
			info("Generate perspective view roadmap\n");
			float csl[2], csr[2], crl[2], crr[2], cry[2], crx[2]; //Coefficient screen/road left-x/right-x of focus region mesh, road y relative to screen y-coord, coefficient road x to screen x
			unsigned int currentPointIdx = -1; //0xFFFFFFFF...
			for (unsigned int y = 0; y < header.height; y++) {
				float yNorm = (float)y / header.height; //Normalized y-coord
				unsigned int requestPointIndex;
				if (yNorm < p[0][0].sy) {
					requestPointIndex = 0;
				} else if (yNorm > p[header.pCnt/2-3][0].sy) {
					requestPointIndex = header.pCnt/2 - 4;
				} else {
					for (unsigned int i = 0; i < header.pCnt/2 - 4; i++) {
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
			unsigned int baselineY = p[header.pCnt/2-1][1].sy * header.height;
			float baselineLeft = t1[ baselineY * header.width + 0 ].px, baselineRight = t1[ baselineY * header.width + header.width - 1 ].px;
			polyFit2(0, header.width - 1, baselineLeft, baselineRight, cx);
			for (unsigned int y = 0; y < header.height; y++) {
				for (unsigned int x = 0; x < header.width; x++) {
					unsigned int i = y * header.width + x;
					t1[i].ox = x * cx[1] + cx[0];
					/* Y-axis in orthographic view is same as in perspective view (we project in x-axis only) */
				}
			}
		}

		/* Perspective pixel width */ {
			for (unsigned int y = 0; y < header.height; y++) {
				unsigned int i = y * header.width;
				unsigned int j = i + 1;
				t1[i].pw = t1[i+1].px - t1[i].px;
				for (unsigned int k = header.width - 1; k; k--) {
					t1[i].pw = t1[i+1].px - t1[i].px;
					i++;
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
		 * will not work. Although 
		 * "for (ivec2 OFFSET; ...; offset += DISTANCE) { xxx = texelFetchOffset(sampler, base + OFFSET, lod); }"
		 * does work, but introduce extra computation per iteration. Then we have to use GLSL code 
		 * "for (ivec2 IDX; idx < LIMIT; idx++) { xxx = texelFetch(sampler, IDX, lod); }". 
		 * 2 - Use NTC instead of absolute coord:
		 * For some reasons, RPI seems not work with isampler. 
		 * 3 - Let the program decide the limit: 
		 * The search distance is the maximum distance that an object could reasonably travel between two frames. 
		 * Since the input video FPS is not known, it doesn't make sense defining the limit now. However, what we 
		 * know now is the up and down edge of the focus region. 
		 */
		float limitLeft = p[header.pCnt/2-2][0].sx, limitRight = p[header.pCnt/2-2][1].sx, limitUp = p[header.pCnt/2-2][1].sy, limitDown = p[header.pCnt/2-1][1].sy;
		info("Limit: left %.4f right %.4f; top %.4f bottom %.4f\n", limitLeft, limitRight, limitUp, limitDown);
		data2_t* ptr = t2;
		for (unsigned int y = 0; y < header.height; y++) {
			float yNorm = (float)y / header.height;
			if ( yNorm < limitUp || yNorm > limitDown ) {
				for (unsigned int x = header.width; x; x--) {
					ptr->searchLimitUp = yNorm;
					ptr->searchLimitDown = yNorm;
					ptr++;
				}
			} else {
				for (unsigned int x = 0; x < header.width; x++) {
					float xNorm = (float)x / header.width;
					if ( xNorm < limitLeft || xNorm > limitRight ) {
						ptr->searchLimitUp = yNorm;
						ptr->searchLimitDown = yNorm;
					} else {
						ptr->searchLimitUp = limitUp;
						ptr->searchLimitDown = limitDown;
					}
					ptr++;
				}
			}
		}
	}

	/* Find lookup table */ {
		info("Finding view transform lookup table\n");
		for (unsigned int y = 0; y < header.height; y++) {
			unsigned int left = y * header.width, right = left + header.width - 1;
			for (unsigned int x = 0; x < header.width; x++) {
				unsigned int current = left + x;

				/* Perspective to Orthographic */ {
					float road = t1[current].ox;
					if (road <= t1[left].px) {
						t2[current].lookupXp2o = 0.0;
					} else if (road >= t1[right].px) {
						t2[current].lookupXp2o = 1.0;
					} else {
						for (unsigned int i = 0; i < header.width - 2; i++) {
							int check = isBetween(road, t1[left+i].px, t1[left+i+1].px);
							if (check != -1) {
								t2[current].lookupXp2o = (float)(i + check) / header.width;
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
						t2[current].lookupXo2p = 1.0;
					} else {
						for (unsigned int i = 0; i < header.width - 2; i++) {
							int check = isBetween(road, t1[left+i].ox, t1[left+i+1].ox);
							if (check != -1) {
								t2[current].lookupXo2p = (float)(i + check) / header.width;
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
		fwrite(p, sizeof(point_t), header.pCnt, stdout);

		fputs("\n\n== Metadata: ===================================================================\n", stdout);
		fprintf(stdout, "Camera resolution: %upx x %upx (%usqpx)\n", header.width, header.height, header.width * header.height);
		for (size_t i = 0; i < header.pCnt / 2; i++) {
			point_t left = p[i][0];
			point_t right = p[i][1];
			unsigned int sxl = left.sx * header.width, sxr = right.sx * header.width, sy = left.sy * header.height;
			fprintf(stdout, "POINT: PL pos (%.4f,%.4f) <%upx,%upx> @ loc (%.4f,%.4f)\n", left.sx, left.sy, sxl, sy, left.rx, left.ry);
			fprintf(stdout, "POINT: PR pos (%.4f,%.4f) <%upx,%upx> @ loc (%.4f,%.4f)\n", right.sx, right.sy, sxr, sy, right.rx, right.ry);
		}
	}

	info("Done!\n");
	return EXIT_SUCCESS;
}