#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef VERBOSE
#include <errno.h>
#endif

#include "roadmap.h"

struct Roadmap_ClassDataStructure {
	roadmap_header header;
	roadmap_t1* t1;
	roadmap_t2* t2;
	unsigned int roadPointsCnt;
	float* roadPoints; //Focus region vertices {screen-x, screen-y}[frVCnt]
};

Roadmap roadmap_init(const char* roadmapFile) {
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
	size_t s = this->header.width * this->header.height;

	this->t1 = malloc(s * sizeof(roadmap_t1));
	this->t2 = malloc(s * sizeof(roadmap_t2));
	if (!this->t1 || !this->t2) {
		#ifdef VERBOSE
			fputs("Fail to create buffer for roadmap tables\n", stderr);
		#endif
		fclose(fp);
		roadmap_destroy(this);
		return NULL;
	}
	if (!fread(this->t1, sizeof(roadmap_t1), s, fp)) {
		#ifdef VERBOSE
			fprintf(stderr, "Error in roadmap file %s: Cannot get table 1\n", roadmapFile);
		#endif
		fclose(fp);
		roadmap_destroy(this);
		return NULL;
	}
	if (!fread(this->t2, sizeof(roadmap_t2), s, fp)) {
		#ifdef VERBOSE
			fprintf(stderr, "Error in roadmap file %s: Cannot get table 2\n", roadmapFile);
		#endif
		fclose(fp);
		roadmap_destroy(this);
		return NULL;
	}

	roadmap_pCnt_t cnt; //Number of POINTS PAIR, in roadmap's data type
	if (!fread(&cnt, sizeof(cnt), 1, fp)) {
		#ifdef VERBOSE
			fprintf(stderr, "Error in roadmap file %s: Cannot get points count\n", roadmapFile);
		#endif
		fclose(fp);
		roadmap_destroy(this);
		return NULL;
	}
	this->roadPointsCnt = cnt * 2; //Numer of POINTS, in native data type

	this->roadPoints = malloc(2 * sizeof(float) * this->roadPointsCnt); //Each POINTS contains 2-axis coords
	if (!this->roadPoints) {
		#ifdef VERBOSE
			fputs("Fail to create buffer for focus region vertices\n", stderr);
		#endif
		fclose(fp);
		roadmap_destroy(this);
		return NULL;
	}
	for (unsigned int i = 0; i < cnt; i++) {
		roadmap_point_t pointPair[2];
		if (!fread(pointPair, sizeof(roadmap_point_t), 2, fp)) {
			#ifdef VERBOSE
				fprintf(stderr, "Error in roadmap file %s: Cannot read points\n", roadmapFile);
			#endif
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