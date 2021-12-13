#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#ifdef VERBOSE
#include <errno.h>
#endif

#include "speedometer.h"

struct Speedometer_ClassDataStructure {
	size2d_t glyphSize;
	uint32_t* glyph;
};

Speedometer speedometer_init(const char* glyphs) {
	Speedometer this = malloc(sizeof(struct Speedometer_ClassDataStructure));
	if (!this) {
		#ifdef VERBOSE
			fputs("Fail to create speedometer class object data structure\n", stderr);
		#endif
		return NULL;
	}
	*this = (struct Speedometer_ClassDataStructure){
		.glyphSize = {0, 0},
		.glyph = NULL
	};

	FILE* fp = fopen(glyphs, "rb");
	if (!fp) {
		#ifdef VERBOSE
			fprintf(stderr, "Fail to open speedometer bitmap file: %s (errno = %d)\n", glyphs, errno);
		#endif
		speedometer_destroy(this);
		return NULL;
	}

	uint8_t glyphSize[2]; //Use 2 * 1 bytes in file
	if (!fread(glyphSize, sizeof(uint8_t), 2, fp)) {
		#ifdef VERBOSE
			fputs("Fail to read speedometer glyph size from file\n", stderr);
		#endif
		fclose(fp);
		speedometer_destroy(this);
		return NULL;
	}
	this->glyphSize = (size2d_t){.width = glyphSize[0], .height = glyphSize[1]}; //Cast from uint8_t[2]

	this->glyph = malloc(256 * this->glyphSize.width * this->glyphSize.height * sizeof(uint32_t));
	if (!this->glyph) {
		#ifdef VERBOSE
			fputs("Fail to create buffer for speedometer glyph\n", stderr);
		#endif
		speedometer_destroy(this);
		return NULL;
	}

	if (!fread(this->glyph, sizeof(uint32_t), 256 * this->glyphSize.width * this->glyphSize.height, fp)) {
		#ifdef VERBOSE
			fputs("Fail to read speedometer glyph from file\n", stderr);
		#endif
		fclose(fp);
		speedometer_destroy(this);
		return NULL;
	}

	fclose(fp);
	return this;
}

int speedometer_convert(Speedometer this) {
	size2d_t size = this->glyphSize;
	size_t totalPixel = 256 * size.width * size.height;
	size_t totalSize = totalPixel * sizeof(uint32_t);

	uint32_t (*temp)[size.height][size.width] = malloc(256 * size.width * size.height * sizeof(uint32_t));
	if (!temp) {
		return 0;
	}

	for (unsigned int oy = 0; oy < 16; oy++) {
		for (unsigned int ox = 0; ox < 16; ox++) {
			for (unsigned int iy = 0; iy < size.height; iy++) {
				for (unsigned int ix = 0; ix < size.width; ix++) {
					unsigned int newIdx = oy * 16 + ox;
					unsigned int oldWidth = size.width * 16;
					unsigned int oldOffsetV = oy * size.height;
					unsigned int oldOffsetH = ox * size.width;
					temp[newIdx][iy][ix] = this->glyph[ (oldOffsetV + iy) * oldWidth + (oldOffsetH + ix)];
				}
			}
		}
	}

	memcpy(this->glyph, temp, 256 * size.width * size.height * sizeof(uint32_t));
	free(temp);
	return 1;
}

uint32_t* speedometer_getGlyph(Speedometer this, size2d_t* size) {
	*size = this->glyphSize;
	return this->glyph;
}

void speedometer_destroy(Speedometer this) {
	if (!this)
		return;

	#ifdef VERBOSE
		fputs("Destroy speedometer class object\n", stdout);
		fflush(stdout);
	#endif

	if (this->glyph)
		free(this->glyph);

	free(this);
}