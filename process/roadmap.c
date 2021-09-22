#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef VERBOSE
#include <errno.h>
#endif

#include "roadmap.h"

struct Roadmap_ClassDataStructure {
	float* vertices; //{screen-x, screen-y}[vCount]
	unsigned int* indices; //{tri1, tri2, tri3}[iCount]
	size_t vCount, iCount;
	float* geographic; //{road-x, road-y, srarch-x, search-y}
	float threshold;
};

Roadmap roadmap_init(const char* focusRegionFile, const char* distanceMapFile, size_t size) {
	#ifdef VERBOSE
		fputs("Init roadmap class object\n", stdout);
		fflush(stdout);
	#endif

	Roadmap this = malloc(sizeof(struct Roadmap_ClassDataStructure));
	if (!this) {
		#ifdef VERBOSE
			fputs("Fail to create roadmap class object data structure\n", stderr);
		#endif
		return NULL;
	}
	*this = (struct Roadmap_ClassDataStructure){
		.vertices = NULL,
		.indices = NULL,
		.vCount = 0,
		.iCount = 0,
		.geographic = NULL,
		.threshold = 0.0f
	};

	FILE* fp;

	/* Focus region */

	fp = fopen(focusRegionFile, "r");
	if (!fp) {
		#ifdef VERBOSE
			fprintf(stderr, "Fail to open focus region map file: %s (errno = %d)\n", focusRegionFile, errno);
		#endif
		roadmap_destroy(this);
		return NULL;
	}

	char buffer[255];

	size_t length;
	for(length = 0; fgets(buffer, sizeof(buffer), fp); length++) {
		if (buffer[0] == 'v')
			this->vCount++;
		else if (buffer[0] == 'i')
			this->iCount++;
		else {
			#ifdef VERBOSE
				fprintf(stderr, "Error in focus region map file (phase-1: count) (line %zu)\n", length);
			#endif
			fclose(fp);
			roadmap_destroy(this);
			return NULL;
		}
	}

	rewind(fp);
	this->vertices = malloc(2 * sizeof(float) * this->vCount);
	this->indices = malloc(3 * sizeof(unsigned int) * this->iCount);
	if (!this->vertices || !this->indices) {
		#ifdef VERBOSE
			fputs("Fail to create buffer for focus region map vertices and/or indices\n", stderr);
		#endif
		fclose(fp);
		roadmap_destroy(this);
		return NULL;
	}

	float* v = this->vertices;
	unsigned int* i = this->indices;
	for(size_t l = 0; l < length; l++) {
		if (fscanf(fp, "i %u %u %u\n", &i[0], &i[1], &i[2]) == 3) {
			i += 3;
		}
		else if (fscanf(fp, "v %f %f\n", &v[0], &v[1]) == 2) {
			v += 2;
		}
		else {
			#ifdef VERBOSE
				fprintf(stderr, "Error in focus region map file (phase-2: fetch) (line %zu)\n", l);
			#endif
			fclose(fp);
			roadmap_destroy(this);
			return NULL;
		}
	}

	fclose(fp);

	/* Geographic data */

	fp = fopen(distanceMapFile, "r");
	if (!fp) {
		#ifdef VERBOSE
			fprintf(stderr, "Fail to open focus region map file: %s (errno = %d)\n", distanceMapFile, errno);
		#endif
		roadmap_destroy(this);
		return NULL;
	}

	this->geographic = malloc(size * sizeof(float) * 4);
	if (!this->geographic) {
		#ifdef VERBOSE
			fputs("Fail to create buffer for geographic map\n", stderr);
		#endif
		fclose(fp);
		roadmap_destroy(this);
		return NULL;
	}

	if (!fread(this->geographic, sizeof(float) * 4, size, fp)) {
		#ifdef VERBOSE
			fputs("Error in geographic map file: Cannot get geographic data\n", stderr);
		#endif
		fclose(fp);
		roadmap_destroy(this);
		return NULL;
	}

	if (!fread(&this->threshold, sizeof(float), 1, fp)) {
		#ifdef VERBOSE
			fputs("Error in geographic map file: Cannot get threshold\n", stderr);
		#endif
		fclose(fp);
		roadmap_destroy(this);
		return NULL;
	}

	fclose(fp);

	return this;
}

float* roadmap_getVertices(Roadmap this, size_t* size) {
	*size = this->vCount;
	return this->vertices;
}

unsigned int* roadmap_getIndices(Roadmap this, size_t* size) {
	*size = this->iCount;
	return this->indices;
}

float* roadmap_getGeographic(Roadmap this) {
	return this->geographic;
}

float roadmap_getThreshold(Roadmap this) {
	return this->threshold;
}

void roadmap_destroy(Roadmap this) {
	if (!this)
		return;

	#ifdef VERBOSE
		fputs("Destroy road class object\n", stdout);
		fflush(stdout);
	#endif

	if (this->vertices)
		free(this->indices);
	if (this->indices)
		free(this->vertices);
	if (this->geographic)
		free(this->geographic);

	free(this);
}