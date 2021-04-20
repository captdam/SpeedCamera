#include <stdlib.h>
#include <math.h>
#include <stdio.h>

#include "compare.h"

#define LUMA_THRESHOLD 10
#define DISTANCE_THRESHOLD 100

struct Compare_ClassDataStructure {
	size2d_t size;
	luma_t* projectSource;
	luma_t* projectBuffer;
	float* distance;
	uint8_t* duration;
};

Compare compare_init(luma_t* project, size2d_t projectSize, float* distance) {
	Compare this = malloc(sizeof(struct Compare_ClassDataStructure));
	if (!this)
		return NULL;
	
	this->size = projectSize;
	this->projectSource = project;
	this->projectBuffer = NULL;
	this->distance = NULL;
	this->duration = NULL;
	
	size_t size = projectSize.width * projectSize.height;
	this->projectBuffer = malloc(size * sizeof(luma_t));
	if (!(this->projectBuffer)) {
		compare_destroy(this);
		return NULL;
	}
	this->distance = malloc(size * sizeof(float));
	if (!(this->distance)) {
		compare_destroy(this);
		return NULL;
	}
	this->duration = calloc(size, sizeof(uint16_t));
	if (!(this->duration)) {
		compare_destroy(this);
		return NULL;
	}
	
	return this;
}

void compare_process(Compare this) {
	luma_t* newProject = this->projectSource;
	luma_t* oldProject = this->projectBuffer;
	uint8_t* duration = this->duration;
	for (size_t y = 0; y < (this->size).height; y++) {
		for (size_t x = 0; x < (this->size).width; x++) {
			*duration = 0;
			if (*(newProject++) > LUMA_THRESHOLD) {
				for (size_t d = 1; d < DISTANCE_THRESHOLD; d++) {
					if (
						oldProject[y*(this->size).width+x + d] > LUMA_THRESHOLD || 
						oldProject[y*(this->size).width+x - d] > LUMA_THRESHOLD || 
						oldProject[(y+d)*(this->size).width+x] > LUMA_THRESHOLD || 
						oldProject[(y-d)*(this->size).width+x] > LUMA_THRESHOLD
					) {
						*duration = d * 30;
//						printf("(%zu,%zu) on: d = %zu\n", x, y, d);
						break;
					}
				}
			}
			duration++;
		}
	}

	newProject = this->projectSource;
	oldProject = this->projectBuffer;
	for (size_t i = (this->size).height * (this->size).width; i > 0; i--)
		*(oldProject++) = *(newProject++);
}

uint8_t* compare_getSpeedMap(Compare this) {
	return this->duration;
}

void compare_destroy(Compare this) {
	if (!this)
		return;
	
	free(this->projectBuffer);
	free(this->distance);
	free(this->duration);
	free(this);
}

#undef LUMA_THRESHOLD
#undef DISTANCE_THRESHOLD