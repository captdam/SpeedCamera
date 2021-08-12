#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef VERBOSE
#include <errno.h>
#endif

#include "roadmap.h"

struct Roadmap_ClassDataStructure {
	float* vertices; //{screen-x, screen-y, road-x, road-y}[vCount]
	unsigned int* indices; //{tri1, tri2, tri3}[iCount]
	size_t vCount, iCount;
};

Roadmap roadmap_init(const char* mapfile) {
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
		.iCount = 0
	};

	FILE* fp = fopen(mapfile, "r");
	if (!fp) {
		#ifdef VERBOSE
			fprintf(stderr, "Fail to open map file: %s (errno = %d)\n", mapfile, errno);
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
			fprintf(stderr, "Error in mapfile (phase-1: count) (line %zu)\n", length);
		}
	}

	rewind(fp);
	this->vertices = malloc(4 * sizeof(float) * this->vCount);
	this->indices = malloc(3 * sizeof(unsigned int) * this->iCount);
	if (!this->vertices || !this->indices) {
		#ifdef VERBOSE
			fputs("Fail to create buffer for roadmap vertices and/or indices\n", stderr);
		#endif
		roadmap_destroy(this);
		return NULL;
	}

	float* v = this->vertices;
	unsigned int* i = this->indices;
	for(size_t l = 0; l < length; l++) {
		if (fscanf(fp, "v %f %f %f %f\n", &v[0], &v[1], &v[2], &v[3]) == 4) {
			v += 4;
		}
		else if (fscanf(fp, "i %u %u %u\n", &i[0], &i[1], &i[2]) == 3) {
			i += 3;
		}
		else {
			#ifdef VERBOSE
				fprintf(stderr, "Error in mapfile (phase-2: fetch) (line %zu)\n", l);
			#endif
			roadmap_destroy(this);
			return NULL;
		}
	}

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

void roadmap_destroy(Roadmap this) {
	if (!this)
		return;

	#ifdef VERBOSE
		fputs("Destroy road class object\n", stdout);
		fflush(stdout);
	#endif

	free(this->indices);
	free(this->vertices);

	free(this);
}