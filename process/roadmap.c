#include <stdlib.h>
#include <stdio.h>

#include "roadmap.h"

struct Roadmap_ClassDataStructure {
	roadmap_header header;
	roadmap_t1* t1;
	roadmap_t2* t2;
	float* roadPoints; //Focus region vertices {screen-x, screen-y}[frVCnt]
};

Roadmap roadmap_init(const char* const roadmapFile, char** const statue) {
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
	unsigned int pixelCount = this->header.width * this->header.height;
	unsigned int sizeHeader = sizeof(roadmap_header);
	unsigned int sizeT1 = pixelCount * sizeof(roadmap_t1);
	unsigned int sizeT2 = pixelCount * sizeof(roadmap_t2);
	unsigned int sizePoints = this->header.pCnt * sizeof(roadmap_point_t);
	if (sizeHeader + sizeT1 + sizeT2 + sizePoints != this->header.fileSize) {
		if (statue)
			*statue = "Calculated file size not match with file header"; //Possible endian issue
		fclose(fp);
		roadmap_destroy(this);
		return NULL;
	}


	this->roadPoints = malloc(2 * sizeof(float) * this->header.pCnt); //Each POINTS contains 2-axis coords
	this->t1 = malloc(pixelCount * sizeof(roadmap_t1));
	this->t2 = malloc(pixelCount * sizeof(roadmap_t2));
	if ( !this->roadPoints || !this->t1 || !this->t2 ) {
		if (statue)
			*statue = "Fail to allocate buffer memory for roadmap data";
		fclose(fp);
		roadmap_destroy(this);
		return NULL;
	}

	if (!fread(this->t1, sizeof(roadmap_t1), pixelCount, fp)) {
		if (statue)
			*statue = "Error in roadmap file: Cannot read table 1";
		fclose(fp);
		roadmap_destroy(this);
		return NULL;
	}
	if (!fread(this->t2, sizeof(roadmap_t2), pixelCount, fp)) {
		if (statue)
			*statue = "Error in roadmap file: Cannot read table 2";
		fclose(fp);
		roadmap_destroy(this);
		return NULL;
	}
	for (float* p = this->roadPoints; p < this->roadPoints + 2 * this->header.pCnt;) {
		roadmap_point_t roadpoint;
		if (!fread(&roadpoint, sizeof(roadmap_point_t), 1, fp)) {
			if (statue)
				*statue = "Error in roadmap file: Cannot read focus region";
			fclose(fp);
			roadmap_destroy(this);
			return NULL;
		}
		*p++ = roadpoint.sx; //Discard road-domain geo coord
		*p++ = roadpoint.sy;
	}

	fclose(fp);
	return this;
}

void roadmap_genLimit(const Roadmap this, float limit) {
	unsigned int width = this->header.width, height = this->header.height;
	for (unsigned int y = 0; y < height; y++) {
		for (unsigned int x = 0; x < width; x++) {
			unsigned int idx = y * width + x;
			float self = this->t1[idx].py;
			int limitUp = y, limitDown = y;
			float limitUpNorm = (float)y / height, limitDownNorm = (float)y / height;
			while(1) {
				if (limitUp <= 0) //Not exceed table size
					break;
				if (limitUpNorm <= this->t2[idx].searchLimitUp) //Not excess max distance provided by roadmap
					break;
				float dest = this->t1[ limitUp * width + x ].py;
				if (dest - self > limit) //Not excess distance limit
					break;
				limitUp--;
				limitUpNorm = (float)limitUp / height;
			}
			while(1) {
				if (limitDown >= height - 1)
					break;
				if (limitDownNorm >= this->t2[idx].searchLimitDown)
					break;
				float dest = this->t1[ limitDown * width + x ].py;
				if (self - dest > limit)
					break;
				limitDown++;
				limitDownNorm = (float)limitDown / height;
			}
			this->t2[ y * width + x ].searchLimitUp = limitUpNorm;
			this->t2[ y * width + x ].searchLimitDown = limitDownNorm;
		}
	}
}

roadmap_header roadmap_getHeader(const Roadmap this) {
	return this->header;
}

roadmap_t1* roadmap_getT1(const Roadmap this) {
	return this->t1;
}

roadmap_t2* roadmap_getT2(const Roadmap this) {
	return this->t2;
}

float* roadmap_getRoadPoints(const Roadmap this) {
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