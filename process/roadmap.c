#include <stdlib.h>
#include <stdio.h>

#include "roadmap.h"

struct Roadmap_ClassDataStructure {
	roadmap_header header;
	roadmap_t1* t1;
	roadmap_t2* t2;
	unsigned int roadPointsCnt;
	float* roadPoints; //Focus region vertices {screen-x, screen-y}[frVCnt]
};

Roadmap roadmap_init(const char* roadmapFile, char** statue) {
	Roadmap this = malloc(sizeof(struct Roadmap_ClassDataStructure));
	if (!this) {
		if (statue)
			*statue = "Fail to create roadmap class object data structure";
		return NULL;
	}
	*this = (struct Roadmap_ClassDataStructure){
		.roadPoints = NULL,
		.t1 = NULL,
		.t2 = NULL
	};

	FILE* fp = fopen(roadmapFile, "rb");
	if (!fp) {
		if (statue)
			*statue = "Fail to open roadmap file";
		roadmap_destroy(this);
		return NULL;
	}
	if (!fread(&this->header, sizeof(roadmap_header), 1, fp)) {
		if (statue)
			*statue = "Error in roadmap file: Cannot read header";
		fclose(fp);
		roadmap_destroy(this);
		return NULL;
	}
	size_t s = this->header.width * this->header.height;

	this->t1 = malloc(s * sizeof(roadmap_t1));
	this->t2 = malloc(s * sizeof(roadmap_t2));
	if (!this->t1 || !this->t2) {
		if (statue)
			*statue = "Fail to allocate buffer memory for roadmap tables";
		fclose(fp);
		roadmap_destroy(this);
		return NULL;
	}
	if (!fread(this->t1, sizeof(roadmap_t1), s, fp)) {
		if (statue)
			*statue = "Error in roadmap file: Cannot read table 1";
		fclose(fp);
		roadmap_destroy(this);
		return NULL;
	}
	if (!fread(this->t2, sizeof(roadmap_t2), s, fp)) {
		if (statue)
			*statue = "Error in roadmap file: Cannot read table 2";
		fclose(fp);
		roadmap_destroy(this);
		return NULL;
	}

	roadmap_pCnt_t cnt; //Number of POINTS PAIR, in roadmap's data type
	if (!fread(&cnt, sizeof(cnt), 1, fp)) {
		if (statue)
			*statue = "Error in roadmap file: Cannot read focus region points count";
		fclose(fp);
		roadmap_destroy(this);
		return NULL;
	}
	this->roadPointsCnt = cnt * 2; //Numer of POINTS, in native data type

	this->roadPoints = malloc(2 * sizeof(float) * this->roadPointsCnt); //Each POINTS contains 2-axis coords
	if (!this->roadPoints) {
		if (statue)
			*statue = "Fail to allocate buffer memory for focus region";
		fclose(fp);
		roadmap_destroy(this);
		return NULL;
	}
	for (unsigned int i = 0; i < cnt; i++) {
		roadmap_point_t pointPair[2];
		if (!fread(pointPair, sizeof(roadmap_point_t), 2, fp)) {
			if (statue)
				*statue = "Error in roadmap file: Cannot read focus region";
			fclose(fp);
			roadmap_destroy(this);
			return NULL;
		}
		this->roadPoints[i * 4 + 0] = pointPair[0].sx;
		this->roadPoints[i * 4 + 1] = pointPair[0].sy;
		this->roadPoints[i * 4 + 2] = pointPair[1].sx;
		this->roadPoints[i * 4 + 3] = pointPair[1].sy;
	}

	fclose(fp);
	return this;
}

roadmap_header roadmap_getHeader(Roadmap this) {
	return this->header;
}

roadmap_t1* roadmap_getT1(Roadmap this) {
	return this->t1;
}

roadmap_t2* roadmap_getT2(Roadmap this) {
	return this->t2;
}

float* roadmap_getRoadPoints(Roadmap this, unsigned int* size) {
	*size = this->roadPointsCnt;
	return this->roadPoints;
}

void roadmap_destroy(Roadmap this) {
	if (!this)
		return;

	free(this->roadPoints);
	free(this->t1);
	free(this->t2);

	free(this);
}