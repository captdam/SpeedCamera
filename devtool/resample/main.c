#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

unsigned int clamp8(int input) {
	if (input > 255) return 255;
	if (input < 0) return 0;
	return input;
}

int main(int argc, char* argv[]) {
	uint8_t (* src)[1920][3] = malloc(1920 * 1080 * 3);
	uint8_t (* dest)[1280][3] = malloc(1280 * 720 * 3);
	
	FILE* fo = fopen("../../v1080.data", "rb");
	FILE* fn = fopen("../../v720.data", "wb");

	size_t progress = 0;

	while (fread(src, 3, 1920 * 1080, fo)) {
		for (size_t y = 0; y < 360; y++) {
			for (size_t x = 0; x < 640; x++) {
				int o[3][3][3]; //3*3 RGB pixels from src, use int to pervent overflow
				for (size_t iy = 0; iy < 3; iy++) {
					for (size_t ix = 0; ix < 3; ix++) {
						for (size_t i = 0; i < 3; i++) {
							o[iy][ix][i] = src[y * 3 + iy][x * 3 + ix][i];
						}
					}
				}

				unsigned int n[2][2][3]; //2*2 RGB pixels to dest
				for (size_t i = 0; i < 3; i++) {
					n[0][0][i] = (4 * o[0][0][i] + 2 * o[0][1][i] + 2 * o[1][0][i] + o[1][1][i]) / 9;
					n[0][1][i] = (4 * o[0][2][i] + 2 * o[0][1][i] + 2 * o[1][2][i] + o[1][1][i]) / 9;
					n[1][0][i] = (4 * o[2][0][i] + 2 * o[2][1][i] + 2 * o[1][0][i] + o[1][1][i]) / 9;
					n[1][1][i] = (4 * o[2][2][i] + 2 * o[2][1][i] + 2 * o[1][2][i] + o[1][1][i]) / 9;
				}

				for (size_t iy = 0; iy < 2; iy++) {
					for (size_t ix = 0; ix < 2; ix++) {
						dest[y * 2 + iy][x * 2 + ix][0] = clamp8(n[iy][ix][2]);
						dest[y * 2 + iy][x * 2 + ix][1] = clamp8(n[iy][ix][1]);
						dest[y * 2 + iy][x * 2 + ix][2] = clamp8(n[iy][ix][0]);
					}
				}
			}
		}
		fwrite(dest, 3, 1280 * 720, fn);
		fprintf(stdout, "\rProgress: %zu", progress++);
		fflush(stdout);
	}

	fclose(fo);
	fclose(fn);
	free(dest);
	free(src);

	return 0;
}