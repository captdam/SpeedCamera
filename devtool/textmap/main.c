#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <errno.h>

#define TEXT_FILE "./src.data"
#define TEXT_WIDTH 12
#define TEXT_HEIGHT 24
/*Source text bitmap order: 0 1 2 3 4 5 6 7 8 9 delimiter, format: RGBA8*/
//Note: GIMP seems cannot export 8-bit gray image, hence we have to switch to the RGBA8 scheme

#define OUTPUT_FILE "./textmap.data"

#define COLOR_IGNORE 2
#define COLOR_LOW 50
#define COLOR_HIGH 120

int main() {
	int statue = EXIT_FAILURE;
	FILE* fp = NULL;
	uint32_t (* src)[TEXT_HEIGHT][TEXT_WIDTH] = NULL; //From source text bitmap: 11 characters (0-9, deli), height (y-axis), width (x-axis)
	uint32_t (* buffer)[4*TEXT_WIDTH] = NULL; //Temp buffer: height, width (4 characters, deli + 3 numbers), RGBA
	uint32_t (* dest)[16 * 4*TEXT_WIDTH]; //Dest: Collection of glphy: 16 * 16 glyphs, each glyph has 4 characters

	/* Allocate memory */
	src = malloc(11 * TEXT_HEIGHT * TEXT_WIDTH * sizeof(uint32_t));
	buffer = malloc(TEXT_HEIGHT * TEXT_WIDTH * 4 * sizeof(uint32_t));
	dest = malloc(16 * TEXT_HEIGHT * 16 * 4 * TEXT_WIDTH * sizeof(uint32_t));
	if (!src || !buffer || !dest) {
		fprintf(stderr, "Cannot allocate memory (errno = %d)\n", errno);
		goto label_exit;
	}

	/* Read source text bitmap file */
	fp = fopen(TEXT_FILE, "rb");
	if (!fp) {
		fprintf(stderr, "Cannot open source text bitmap file (errno = %d)\n", errno);
		goto label_exit;
	}
	if (!fread(src, TEXT_HEIGHT * TEXT_WIDTH * sizeof(uint32_t), 11, fp)) {
		fprintf(stderr, "Cannot read source text bitmap file (errno = %d)\n", errno);
		goto label_exit;
	}
	for (size_t i = 0; i < 11; i++) {
		for (size_t y = 0; y < TEXT_HEIGHT; y++) {
			for (size_t x = 0; x < TEXT_WIDTH; x++) {
				src[i][y][x] &= 0x00FFFFFF; //Mask used to remove alpha channel from the source bitmap
			}
		}
	}
	fclose(fp);
	fp = NULL;

	/* Open output file */
	fp = fopen(OUTPUT_FILE, "wb");
	if (!fp) {
		fprintf(stderr, "Cannot create/open output file (errno = %d)\n", errno);
		goto label_exit;
	}

	/* Gen output */
	for (int i = 0; i < 256; i++) {
		uint8_t r, g, b, a;
		if (i < COLOR_IGNORE) {
			r = 0; //Same as cn
			g = 0;
			b = 0;
			a = 0x80;
		}
		else if (i <= COLOR_LOW) {
			r = 0;
			g = 255;
			b = 0;
			a = 255;
		}
		else if (i >= COLOR_HIGH) {
			r = 255;
			g = 0;
			b = 0;
			a = 255;
		}
		else {
			float range = COLOR_HIGH - COLOR_LOW;
			float offset = i - COLOR_LOW;
			r = 255.0f * offset / range;
			g = 255 - r;
			b = 0;
			a = 255;
		}
		uint32_t cy = (r << 0) | (g << 8) | (b << 16) | (a << 24); //Little endian: address 0 = alpha, address 3 = red
		uint32_t cn = 0x80000000;

		int  c0 = 10, c1 = i / 100, c2 = (i / 10) % 10, c3 = i % 10; //Characters: char 0 = deli(10), char 1-3 = value

		for (int y = 0; y < TEXT_HEIGHT; y++) {
			for (int x = 0; x < TEXT_WIDTH; x++) {
				buffer[y][3 * TEXT_WIDTH + x] = src[c3][y][x]? cy : cn;
				buffer[y][2 * TEXT_WIDTH + x] = src[c2][y][x]? cy : cn;
				buffer[y][1 * TEXT_WIDTH + x] = src[c1][y][x]? cy : cn;
				buffer[y][0 * TEXT_WIDTH + x] = src[c0][y][x]? cy : cn;
			}
		}
		
		size_t offsetY = TEXT_HEIGHT * (i / 16), offsetX = 4 * TEXT_WIDTH * (i % 16);
		for (size_t y = 0; y < TEXT_HEIGHT; y++) {
			for (size_t x = 0; x < 4 * TEXT_WIDTH; x++) {
				dest[offsetY + y][offsetX + x] = buffer[y][x];
			}
		}
		
	}

	fwrite(dest, sizeof(uint32_t), 16 * TEXT_HEIGHT * 16 * 4 * TEXT_WIDTH, fp);
	fclose(fp);
	fp = NULL;

	statue = EXIT_SUCCESS;
	label_exit:
	if (src) free(src);
	if (buffer) free(buffer);
	if (fp) fclose(fp);
	return statue;
}