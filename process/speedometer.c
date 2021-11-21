#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

#ifdef VERBOSE
#include <errno.h>
#endif

#include "speedometer.h"

struct Speedometer_ClassDataStructure {
	size2d_t glyphSize;
	uint32_t* glyph;
	float* vertices;
	unsigned int* indices;
	size2d_t count;
};

Speedometer speedometer_init(const char* glyphs, size2d_t count) {
	#ifdef VERBOSE
		fputs("Init speedometer class object\n", stdout);
		fflush(stdout);
	#endif

	Speedometer this = malloc(sizeof(struct Speedometer_ClassDataStructure));
	if (!this) {
		#ifdef VERBOSE
			fputs("Fail to create speedometer class object data structure\n", stderr);
		#endif
		return NULL;
	}
	*this = (struct Speedometer_ClassDataStructure){
		.glyphSize = {0, 0},
		.glyph = NULL,
		.vertices = NULL,
		.indices = NULL,
		.count = {0, 0}
	};

	/* Load glyph from file */ {
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
	}

	/* Generate location and sample region of each speedometer base on the given param 'count' (number of meters) */ {
		this->vertices = malloc(count.width * count.height * 4 * sizeof(float));
		this->indices = malloc(count.width * count.height * sizeof(unsigned int));
		if (!this->vertices || !this->indices) {
			#ifdef VERBOSE
				fputs("Fail to create buffer for speedometer vertices and indices\n", stderr);
			#endif
			speedometer_destroy(this);
			return NULL;
		}

		/** Note: Why not use instanced draw and geometry shader?
		 * For compatibility and simplicity, we use the most basic method to draw the speedometer. 
		 * - Instanced draw requires a new vbo, means we have to modify the orginal gl lib. 
		 * - Instanced draw is introduced in GLES3.1 core, old GPU/driver will not support it, new driver may emulate it. 
		 * - Geometry shader is introduced in GLES3.1 core, old GPU/driver will not support it, new driver may emulate it. 
		 * - Our speedometer is small, we only have few vertices, standard draw can handle this load well. 
		 */

		this->count = count;
		float sizeH = 1.0 / count.width, sizeV = 1.0 / count.height;
		float* vptr = this->vertices;
		unsigned int* iptr = this->indices;
		unsigned int idx = 0;
		for (unsigned int y = 0; y < count.height; y++) {
			for (unsigned int x = 0; x < count.width; x++) {
				*(vptr++) = sizeH * x;		//Left limit (p1x)
				*(vptr++) = sizeV * y;		//Top limit (p1y)
				*(vptr++) = sizeH * x + sizeH;	//Right limit (p2x)
				*(vptr++) = sizeV * y + sizeV;	//Bottom limit (p2y)
				*(iptr++) = idx;
			}
		}
	}

	return this;
}

uint32_t* speedometer_getGlyph(Speedometer this, size2d_t* size) {
	*size = this->glyphSize;
	return this->glyph;
}

float* speedometer_getVertices(Speedometer this, size_t* size) {
	*size = this->count.width * this->count.height;
	return this->vertices;
}

unsigned int* speedometer_getIndices(Speedometer this, size_t* size) {
	*size = this->count.width * this->count.height;
	return this->indices;
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
	if (this->vertices)
		free(this->vertices);
	if (this->indices)
		free(this->indices);

	free(this);
}