#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef VERBOSE
#include <errno.h>
#endif

#include "roadmap.h"

struct Roadmap_ClassDataStructure {
	float* roadPoints; //Focus region vertices {screen-x, screen-y}[frVCnt]
	size_t roadPointsCnt;
	roadmap_header header;
	roadmap_t1* t1;
	roadmap_t2* t2;
};

Roadmap roadmap_init(const char* roadmapFile, size2d_t size) {
	Roadmap this = malloc(sizeof(struct Roadmap_ClassDataStructure));
	if (!this) {
		#ifdef VERBOSE
			fputs("Fail to create roadmap class object data structure\n", stderr);
		#endif
		return NULL;
	}
	*this = (struct Roadmap_ClassDataStructure){
		.roadPoints = NULL,
		.t1 = NULL,
		.t2 = NULL
	};

	FILE* fp = fopen(roadmapFile, "rb");
	if (!fp) {
		#ifdef VERBOSE
			fprintf(stderr, "Fail to open roadmap file: %s (errno = %d)\n", roadmapFile, errno);
		#endif
		roadmap_destroy(this);
		return NULL;
	}

	if (!fread(&this->header, sizeof(roadmap_header), 1, fp)) {
		#ifdef VERBOSE
			fprintf(stderr, "Error in roadmap file %s: Cannot get header\n", roadmapFile);
		#endif
		fclose(fp);
		roadmap_destroy(this);
		return NULL;
	}

	if (this->header.width != size.width || this->header.height != size.height) {
		#ifdef VERBOSE
			fprintf(stderr, "Error in roadmap file %s: Size mismatched\n", roadmapFile);
		#endif
		fclose(fp);
		roadmap_destroy(this);
		return NULL;
	}

	this->t1 = malloc(size.width * size.height * sizeof(roadmap_t1));
	this->t2 = malloc(size.width * size.height * sizeof(roadmap_t2));
	if (!this->t1 || !this->t2) {
		#ifdef VERBOSE
			fputs("Fail to create buffer for roadmap tables\n", stderr);
		#endif
		fclose(fp);
		roadmap_destroy(this);
		return NULL;
	}

	if (!fread(this->t1, sizeof(roadmap_t1), size.width * size.height, fp)) {
		#ifdef VERBOSE
			fprintf(stderr, "Error in roadmap file %s: Cannot get table 1\n", roadmapFile);
		#endif
		fclose(fp);
		roadmap_destroy(this);
		return NULL;
	}

	if (!fread(this->t2, sizeof(roadmap_t2), size.width * size.height, fp)) {
		#ifdef VERBOSE
			fprintf(stderr, "Error in roadmap file %s: Cannot get table 2\n", roadmapFile);
		#endif
		fclose(fp);
		roadmap_destroy(this);
		return NULL;
	}

	unsigned int pCount = 0;
	if (!fread(&pCount, sizeof(unsigned int), 1, fp)) {
		#ifdef VERBOSE
			fprintf(stderr, "Error in roadmap file %s: Cannot get points count\n", roadmapFile);
		#endif
		fclose(fp);
		roadmap_destroy(this);
		return NULL;
	}
	this->roadPointsCnt = pCount * 2;

	this->roadPoints = malloc(2 * sizeof(float) * this->roadPointsCnt);
	if (!this->roadPoints) {
		#ifdef VERBOSE
			fputs("Fail to create buffer for focus region vertices\n", stderr);
		#endif
		fclose(fp);
		roadmap_destroy(this);
		return NULL;
	}

	for (unsigned int i = 0; i < pCount; i++) {
		roadmap_point_t pointPair[2];
		if (!fread(pointPair, sizeof(roadmap_point_t), 2, fp)) {
			#ifdef VERBOSE
				fprintf(stderr, "Error in roadmap file %s: Cannot read points\n", roadmapFile);
			#endif
			fclose(fp);
			roadmap_destroy(this);
			return NULL;
		}

		float leftX = pointPair[0].screenX / (float)size.width;
		float leftY = pointPair[0].screenY / (float)size.height;
		float rightX = pointPair[1].screenX / (float)size.width;
		float rightY = pointPair[1].screenY / (float)size.height;
		this->roadPoints[i * 2 + 0] = leftX;
		this->roadPoints[i * 2 + 1] = leftY;
		this->roadPoints[pCount * 4 - i * 2 - 2] = rightX;
		this->roadPoints[pCount * 4 - i * 2 - 1] = rightY;
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

float* roadmap_getRoadPoints(Roadmap this, size_t* size) {
	*size = this->roadPointsCnt;
	return this->roadPoints;
}

void roadmap_destroy(Roadmap this) {
	if (!this)
		return;

	#ifdef VERBOSE
		fputs("Destroy roadmap class object\n", stdout);
		fflush(stdout);
	#endif

	if (this->roadPoints)
		free(this->roadPoints);
	if (this->t1)
		free(this->t1);
	if (this->t2)
		free(this->t2);

	free(this);
}