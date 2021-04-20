#include <stdlib.h>
#include <math.h>

#include "compare.h"

#define LUMA_THRESHOLD 10

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
	for (size_t i = (this->size).width * (this->size).height; i > 0; i--) {
		int new = *(newProject++);
		int old = *(oldProject++);

//		if (new < 0x20 && old > 0x20) { //Leave
//			*(duration++) = 0;
//		}
//		else if (new > 0x20 && old < 0x20) { //Enter
//			*duration = *duration + 20;
//			duration++;
//		}
//		else if (new > 0x20 && old > 0x20) { //Stay
//			*duration = *duration + 20;
//			duration++;
//		}
//		else /*new < 0x20 && old < 0x20*/ { //Empty
//			*(duration++) = 0;
//		}

		if (new < LUMA_THRESHOLD && old >= LUMA_THRESHOLD) { //Leave
			*duration = 0; //Reset
		}
		else if (new >= LUMA_THRESHOLD && old < LUMA_THRESHOLD) { //Enter
			*duration = -30;
		}
		else if (new >= LUMA_THRESHOLD && old >= LUMA_THRESHOLD) { //Stay
			*duration = *duration - 30;
		}
		else /*new < LUMA_THRESHOLD && old < LUMA_THRESHOLD*/ { //Empty
			*duration = 0; //Reset
		}

		duration++;
	}
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