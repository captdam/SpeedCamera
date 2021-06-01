#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <stdio.h>

#include "compare.h"

#define LUMA_THRESHOLD 40

typedef struct RoadNeighbor {
	unsigned int distance: 8;
	unsigned int pos: 24;
} neighbor_t;
#define NEIGHBOR_DIS_MAX 255
#define NEIGHBOR_POS_MAX 16777215

typedef struct RoadPoint {
	neighbor_t* neighborStart;
	neighbor_t* neighborEnd;
} road_t;

struct Compare_ClassDataStructure {
	size2d_t size;
	uint8_t* previousFrame;
	road_t* roadPoint;
	neighbor_t* neighborMap;
};

Compare compare_init(const size2d_t size, const char* mapfile) {
	Compare this = malloc(sizeof(struct Compare_ClassDataStructure));
	if (!this)
		return NULL;
	
	this->size = size;
	this->previousFrame = NULL;
	this->roadPoint = NULL;
	this->neighborMap = NULL;

	//Open neighbor map
	FILE* fp = fopen(mapfile, "rb");
	if (!fp) {
		compare_destroy(this);
		return NULL;
	}
	uint32_t neighborCount;
	if(!fread(&neighborCount, 1, sizeof(neighborCount), fp)) {
		compare_destroy(this);
		return NULL;
	}

	//Allocate memory
	size_t area = size.width * size.height;
	this->previousFrame = malloc(area * sizeof(*this->previousFrame)); 
	if (!this->previousFrame) {
		compare_destroy(this);
		return NULL;
	}
	this->roadPoint = malloc(area * sizeof(*this->roadPoint)); 
	if (!this->roadPoint) {
		compare_destroy(this);
		return NULL;
	}
	this->neighborMap = malloc((size_t)neighborCount * sizeof(*this->neighborMap)); 
	if (!this->neighborMap) {
		compare_destroy(this);
		return NULL;
	}

	//Init road point map (pointer to neighbor points)
	road_t* roadPointWritePtr = this->roadPoint;
	for (size_t i = area; i; i--) {
		uint32_t neighborIndex[2];
		if(!fread(&neighborIndex, 1, sizeof(neighborIndex), fp)) {
			compare_destroy(this);
			return NULL;
		}
		*(roadPointWritePtr++) = (road_t){
			.neighborStart = &this->neighborMap[ neighborIndex[0] ],
			.neighborEnd = &this->neighborMap[ neighborIndex[0] + neighborIndex[1] ]
		};
	}

	//Load neighbor map
	if (fread(this->neighborMap, sizeof(*this->neighborMap), neighborCount, fp) != neighborCount) {
		compare_destroy(this);
		return NULL;
	}

	fclose(fp);
	return this;
}

void compare_process(Compare this, uint8_t* dest, uint8_t* src) {
	size_t area = this->size.height * this->size.width;
	memset(dest, 0, area * sizeof(*dest));

	for (size_t i = area; i; i--) {
		if (!src[i])
			continue;
		if (this->previousFrame[i])
			continue;
//		if (src[i] - this->previousFrame[i] < LUMA_THRESHOLD)
//			continue;

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
	
	free(this->previousFrame);
	free(this->roadPoint);
	free(this->neighborMap);
	free(this);
}