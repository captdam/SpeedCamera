#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <stdio.h>

#include "compare.h"

#define LUMA_THRESHOLD 40

struct Compare_ClassDataStructure {
	size2d_t size;
	uint8_t* previousFrame;
	road_t* roadPoint;
};

Compare compare_init(const size2d_t size, road_t* roadPoint) {
#ifdef VERBOSE
	fputs("Init compare class object\n", stdout);
	fflush(stdout);
#endif

	Compare this = malloc(sizeof(struct Compare_ClassDataStructure));
	if (!this) {
#ifdef VERBOSE
		fputs("\tFail to create compare class object data structure\n", stderr);
#endif
		return NULL;
	}
	
	this->size = size;
	this->previousFrame = NULL;
	this->roadPoint = roadPoint;

	this->previousFrame = malloc(size.width * size.height * sizeof(*this->previousFrame)); 
	if (!this->previousFrame) {
#ifdef VERBOSE
		fputs("\tCannot allocate buffer for previous frame\n", stderr);
#endif
		compare_destroy(this);
		return NULL;
	}

	return this;
}

void compare_process(Compare this, uint8_t* dest, uint8_t* src) {
	size_t area = this->size.height * this->size.width;
	memset(dest, 0, area * sizeof(*dest));

	for (size_t i = 0; i < area; i++) {
		if (!this->roadPoint[i].neighborStart)
			continue;
		if (src[i] < LUMA_THRESHOLD)
			continue;
		if (this->previousFrame[i] > LUMA_THRESHOLD)
			continue;
		if (src[i] - this->previousFrame[i] < LUMA_THRESHOLD)
			continue;

		for (neighbor_t* x = this->roadPoint[i].neighborStart; x < this->roadPoint[i].neighborEnd; x++) {
			if (this->previousFrame[ x->pos ]) {
				dest[i] = x->distance;
				break;
			}
		}
	}
	
	memcpy(this->previousFrame, src, area * sizeof(*src));
}

void compare_destroy(Compare this) {
	if (!this)
		return;
	
#ifdef VERBOSE
	fputs("Destroy compare class object\n", stdout);
	fflush(stdout);
#endif
	free(this->previousFrame);
	free(this);
}