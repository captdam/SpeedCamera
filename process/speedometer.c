#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

#ifdef VERBOSE
#include <errno.h>
#endif

#include "speedometer.h"

struct Speedometer_ClassDataStructure {
	uint8_t* bitmap;
	float* vertices;
	unsigned int* indices;
	float* offsets;
	size2d_t size;
	size2d_t count;
};

Speedometer speedometer_init(const char* bitmapfile, size2d_t size, size2d_t count) {
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
		.bitmap = NULL,
		.vertices = NULL,
		.indices = NULL,
		.size = {0, 0},
		.count = {0, 0}
	};

	/** Note: Why not use instanced draw?
	 * - Instanced draw requires a new vbo, means we have to modify the orginal gl lib. 
	 * - Instanced draw is introduced in GLES3.1, old GPU/driver will not support it, new driver may emulate it. 
	 * - Our speedometer is small, we only have count * 2 faces, standard draw can handle this load well. 
	 */

	this->vertices = malloc(count.width * count.height * 4 * 8 * sizeof(float));
	this->indices = malloc(count.width * count.height * 2 * 3 * sizeof(unsigned int));
	if (!this->vertices || !this->indices) {
		#ifdef VERBOSE
			fputs("Fail to create buffer for speedometer vertices and indices\n", stderr);
		#endif
		speedometer_destroy(this);
		return NULL;
	}

	/* Generate vertices and indices array base on the given param count */ {
		float sizeH = 1.0 / count.width, sizeV = 1.0 / count.height;
		float* vptr = this->vertices;
		unsigned int* iptr = this->indices;
		for (unsigned int y = 0; y < count.height; y++) {
			for (unsigned int x = 0; x < count.width; x++) {
				float centerH = -1.0 + sizeH + sizeH * 2 * x;
				float centerV = -1.0 + sizeV + sizeV * 2 * y;
				*(vptr++) = centerH + sizeH; //Top-right - Screen position
				*(vptr++) = centerV + sizeV;
				*(vptr++) = 1.0f; //Top-right - Texture coord
				*(vptr++) = 1.0f;
				*(vptr++) = x * sizeH; *(vptr++) = (x + 1) * sizeH; //Top-right - Sample region
				*(vptr++) = y * sizeV; *(vptr++) = (y + 1) * sizeV;
				*(vptr++) = centerH + sizeH; //Bottom-right
				*(vptr++) = centerV - sizeV;
				*(vptr++) = 1.0f;
				*(vptr++) = 0.0f;
				*(vptr++) = x * sizeH; *(vptr++) = (x + 1) * sizeH;
				*(vptr++) = y * sizeV; *(vptr++) = (y + 1) * sizeV;
				*(vptr++) = centerH - sizeH; //Bottom-left
				*(vptr++) = centerV - sizeV;
				*(vptr++) = 0.0f;
				*(vptr++) = 0.0f;
				*(vptr++) = x * sizeH; *(vptr++) = (x + 1) * sizeH;
				*(vptr++) = y * sizeV; *(vptr++) = (y + 1) * sizeV;
				*(vptr++) = centerH - sizeH; //Top-left
				*(vptr++) = centerV + sizeV;
				*(vptr++) = 0.0f;
				*(vptr++) = 1.0f;
				*(vptr++) = x * sizeH; *(vptr++) = (x + 1) * sizeH;
				*(vptr++) = y * sizeV; *(vptr++) = (y + 1) * sizeV;

				unsigned int base = (y * count.width + x) * 4;
				*(iptr++) = base + 0;
				*(iptr++) = base + 3;
				*(iptr++) = base + 2;
				*(iptr++) = base + 0;
				*(iptr++) = base + 2;
				*(iptr++) = base + 1;
			}
		}
	}

	this->bitmap = malloc(size.width * size.height * sizeof(uint32_t));
	if (!this->bitmap) {
		#ifdef VERBOSE
			fputs("Fail to create buffer for speedometer bitmap\n", stderr);
		#endif
		speedometer_destroy(this);
		return NULL;
	}

	FILE* fp = fopen(bitmapfile, "rb");
	if (!fp) {
		#ifdef VERBOSE
			fprintf(stderr, "Fail to open speedometer bitmap file: %s (errno = %d)\n", bitmapfile, errno);
		#endif
		speedometer_destroy(this);
		return NULL;
	}

	if(!fread(this->bitmap, sizeof(uint32_t), size.width * size.height, fp)) {
		#ifdef VERBOSE
			fputs("Fail to read speedometer bitmap from file\n", stderr);
		#endif
		fclose(fp);
		speedometer_destroy(this);
		return NULL;
	}
	fclose(fp);

	this->size = size;
	this->count = count;
	return this;
}

void* speedometer_getBitmap(Speedometer this) {
	return this->bitmap;
}

float* speedometer_getVertices(Speedometer this, size_t* size) {
	*size = 4 * this->count.width * this->count.height;
	return this->vertices;
}

unsigned int* speedometer_getIndices(Speedometer this, size_t* size) {
	*size = 2 * this->count.width * this->count.height;
	return this->indices;
}

void speedometer_destroy(Speedometer this) {
	if (!this)
		return;

	#ifdef VERBOSE
		fputs("Destroy speedometer class object\n", stdout);
		fflush(stdout);
	#endif

	if (this->bitmap)
		free(this->bitmap);

	free(this);
}