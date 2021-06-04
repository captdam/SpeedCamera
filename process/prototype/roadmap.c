#include <stdlib.h>
#include <math.h>
#include <string.h>

#ifdef VERBOSE
#include <stdio.h>
#include <errno.h>
#endif

#include "roadmap.h"

struct Roadmap_ClassDataStructure {
	road_t* roadPoint;
	neighbor_t* neighborMap;
};

Roadmap roadmap_init(const char* mapFile, size2d_t size) {
#ifdef VERBOSE
	fputs("Init roadmap class object\n", stdout);
	fflush(stdout);
#endif

	if (size.width * size.height > ROADMAP_POS_MAX) {
#ifdef VERBOSE
		fputs("\tFrame size too large\n", stderr);
#endif
		return NULL;
	}

	Roadmap this = malloc(sizeof(struct Roadmap_ClassDataStructure));
	if (!this) {
#ifdef VERBOSE
		fputs("\tFail to create roadmap class object data structure\n", stderr);
#endif
		return NULL;
	}
	
	this->roadPoint = NULL;
	this->neighborMap = NULL;

	FILE* fp = fopen(mapFile, "rb");
	if (!fp) {
#ifdef VERBOSE
		fprintf(stderr, "\tFail to open roadmap file %s (errno = %d)\n", mapFile, errno);
#endif
		roadmap_destroy(this);
		return NULL;
	}
	uint32_t neighborCount;
	if(!fread(&neighborCount, 1, sizeof(neighborCount), fp)) {
#ifdef VERBOSE
		fputs("\tCannot get neighbor points list size\n", stderr);
#endif
		roadmap_destroy(this);
		return NULL;
	}

	this->roadPoint = malloc(size.width * size.height * sizeof(*this->roadPoint)); 
	if (!this->roadPoint) {
#ifdef VERBOSE
		fputs("\tCannot allocate buffer for road points list\n", stderr);
#endif
		roadmap_destroy(this);
		return NULL;
	}
	this->neighborMap = malloc(neighborCount * sizeof(*this->neighborMap)); 
	if (!this->neighborMap) {
#ifdef VERBOSE
		fputs("\tCannot allocate buffer for neighbor points list\n", stderr);
#endif
		roadmap_destroy(this);
		return NULL;
	}

	for (road_t* ptr = this->roadPoint; ptr < this->roadPoint + size.height * size.width; ptr++) {
		uint32_t neighborIndex[2];
		if(!fread(&neighborIndex, 1, sizeof(neighborIndex), fp)) {
#ifdef VERBOSE
			fputs("\tRoad points list bad data\n", stderr);
#endif
			roadmap_destroy(this);
			return NULL;
		}
		if (neighborIndex[1] == 0) { //Out of focus region
			*ptr = (road_t){
				.neighborStart = NULL,
				.neighborEnd = NULL
			};
		}
		else {
			*ptr = (road_t){
				.neighborStart = this->neighborMap + neighborIndex[0],
				.neighborEnd = this->neighborMap + neighborIndex[0] + neighborIndex[1]
			};
		}
	}

	if (fread(this->neighborMap, sizeof(*this->neighborMap), neighborCount, fp) != neighborCount) {
#ifdef VERBOSE
		fputs("\tNeighbor points list bad data\n", stderr);
#endif
		roadmap_destroy(this);
		return NULL;
	}

	fclose(fp);
	return this;
}

road_t* roadmap_getRoadpoint(Roadmap this) {
	return this->roadPoint;
}

void roadmap_destroy(Roadmap this) {
	if (!this)
		return;

#ifdef VERBOSE
	fputs("Destroy roadmap class object\n", stdout);
	fflush(stdout);
#endif
	free(this->roadPoint);
	free(this->neighborMap);
	free(this);
}