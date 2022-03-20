#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#ifdef VERBOSE
#include <errno.h>
#endif

#include "speedometer.h"

struct Speedometer_ClassDataStructure {
	uint8_t size[2];
	uint32_t* glyph;
};

Speedometer speedometer_init(const char* glyphs, char** statue) {
	Speedometer this = malloc(sizeof(struct Speedometer_ClassDataStructure));
	if (!this) {
		if (statue)
			*statue = "Fail to create speedometer class object data structure";
		return NULL;
	}
	*this = (struct Speedometer_ClassDataStructure){ .glyph = NULL };

	FILE* fp = fopen(glyphs, "rb");
	if (!fp) {
		if (statue)
			*statue = "Fail to open speedometer glyph file";
		speedometer_destroy(this);
		return NULL;
	}

	if (!fread(&this->size, 1, 2, fp)) {
		if (statue)
			*statue = "Error in speedometer glyph file: Cannot read size";
		fclose(fp);
		speedometer_destroy(this);
		return NULL;
	}

	this->glyph = malloc(256 *  this->size[0] * this->size[1] * sizeof(uint32_t));
	if (!this->glyph) {
		if (statue)
			*statue = "Fail to create buffer for speedometer glyph";
		fclose(fp);
		speedometer_destroy(this);
		return NULL;
	}

	if (!fread(this->glyph, sizeof(uint32_t), 256 *  this->size[0] * this->size[1], fp)) {
		if (statue)
			*statue = "Error in speedometer glyph file: Cannot read glyph";
		fclose(fp);
		speedometer_destroy(this);
		return NULL;
	}

	fclose(fp);
	return this;
}

int speedometer_convert(Speedometer this) {
	unsigned int gWidth = this->size[0], gHeight = this->size[1];
	uint32_t temp[16][gHeight][gWidth]; //For a row, 16 char

	/* Orginal format
	Ch0     Ch1     Ch2     Ch3     Ch4     Ch6     ... 16 characters
	AAAAAAA BBBBBBB CCCCCCC DDDDDDD EEEEEEE FFFFFFF ... - 0
	AAAAAAA BBBBBBB CCCCCCC DDDDDDD EEEEEEE FFFFFFF ... h y
	AAAAAAA BBBBBBB XCCCCCC DDDDDDD EEEEEEE FFFFFFF ... | |
	AAAAAAA BBBBBBB CCCCCCC DDDDDDD EEEEEEE FFFFFFF ... _ h
	|--w--| |--w--| |--w--| |--w--| |--w--| |--w--|
	0  x  w
	X (Ch, y, x) = y * (w * 16) + Ch * w + x
	*/

	for (unsigned int row = 0; row < 16; row++) { //16 rows * 16 characters/row
		uint32_t* rowPtr = this->glyph + row * 16 * gHeight * gWidth;
		for (unsigned int ch = 0; ch < 16; ch++) {
			for (unsigned int y = 0; y < gHeight; y++) {
				uint32_t* src = rowPtr + y * gWidth * 16 + ch * gWidth;
				memcpy(temp[ch][y], src, gWidth * sizeof(uint32_t));
			}
		}
		memcpy(rowPtr, temp, sizeof(temp));
	}

	return 1;
}

uint32_t* speedometer_getGlyph(Speedometer this, unsigned int size[static 2]) {
	size[0] = this->size[0];
	size[1] = this->size[1];
	return this->glyph;
}

void speedometer_destroy(Speedometer this) {
	if (!this)
		return;

	free(this->glyph);
	free(this);
}