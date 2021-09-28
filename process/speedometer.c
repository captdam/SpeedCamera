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
	size3d_t size;
	size2d_t count;
};

Speedometer speedometer_init(const char* bitmap, size3d_t size, size2d_t count) {
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
		.offsets = NULL,
		.size = {0, 0, 0},
		.count = {0, 0}
	};

	this->vertices = malloc(4 * 2 * sizeof(float));
	this->indices = malloc(2 * 3 * sizeof(unsigned int));
	this->offsets = malloc(count.width * count.height * 2 * sizeof(float));
	if (!this->vertices || !this->indices || !this->offsets) {
		#ifdef VERBOSE
			fputs("Fail to create buffer for speedometer vertices, indices and offsets\n", stderr);
		#endif
		speedometer_destroy(this);
		return NULL;
	}

	float sizeH = 1.0 / count.width, sizeV = 1.0 / count.height;

	this->vertices[0] = +sizeH; this->vertices[1] = +sizeV;
	this->vertices[2] = +sizeH; this->vertices[3] = -sizeV;
	this->vertices[4] = -sizeH; this->vertices[5] = -sizeV;
	this->vertices[6] = -sizeH; this->vertices[7] = +sizeV;

	this->indices[0] = 0; this->indices[1] = 3; this->indices[2] = 2;
	this->indices[3] = 0; this->indices[4] = 2; this->indices[5] = 1;

	for (unsigned int y = 0; y < count.height; y++) {
		for (unsigned int x = 0; x < count.width; x++) {
			unsigned int index = y * count.width + x;
			this->offsets[(index << 1) + 0] = -1.0 + sizeH + sizeH * 2 * x;
			this->offsets[(index << 1) + 1] = -1.0 + sizeV + sizeV * 2 * y;
		}
	}

	this->bitmap = malloc(size.width * size.height * size.depth * sizeof(uint32_t));
	if (!this->bitmap) {
		#ifdef VERBOSE
			fputs("Fail to create buffer for speedometer bitmap\n", stderr);
		#endif
		speedometer_destroy(this);
		return NULL;
	}

	FILE* fp = fopen(bitmap, "rb");
	if (!fp) {
		#ifdef VERBOSE
			fprintf(stderr, "Fail to open speedometer bitmap file: %s (errno = %d)\n", bitmap, errno);
		#endif
		speedometer_destroy(this);
		return NULL;
	}

	if(!fread(this->bitmap, sizeof(uint32_t), size.width * size.height * size.depth, fp)) {
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
	*size = 4;
	return this->vertices;
}

unsigned int* speedometer_getIndices(Speedometer this, size_t* size) {
	*size = 2;
	return this->indices;
}

float* speedometer_getOffsets(Speedometer this, size_t* size) {
	*size = this->count.width * this->count.height;
	return this->offsets;
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