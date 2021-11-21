#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef VERBOSE
#include <errno.h>
#endif

#include "roadmap.h"

struct Roadmap_ClassDataStructure {
	float* frVertices; //{screen-x, screen-y}[frVCnt]
	unsigned int* frIndices; //{tri1, tri2, tri3}[frICnt]
	float* pVertices; //{prospectiveXY, orthographicXY}[pVCnt]
	unsigned int* pIndices; //{tri1, tri2, tri3}[pICnt]
	size_t frVCnt, frICnt, pVCnt, pICnt;
	float* geographic; //{road-x, road-y, srarch-x, search-y}
	float threshold;
};

Roadmap roadmap_init(const char* focusRegionFile, const char* projectFile, const char* distanceMapFile, size_t size) {
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
		.frVertices = NULL,	.frIndices = NULL,	.frVCnt = 0,	.frICnt = 0,
		.pVertices = NULL,	.pIndices = NULL,	.pVCnt = 0,	.pICnt = 0,
		.geographic = NULL,	.threshold = 0.0f
	};

	FILE* fp = NULL;
	char buffer[127];

	/* Focus region */ {
		fp = fopen(focusRegionFile, "r");
		if (!fp) {
			#ifdef VERBOSE
				fprintf(stderr, "Fail to open focus region map file: %s (errno = %d)\n", focusRegionFile, errno);
			#endif
			roadmap_destroy(this);
			return NULL;
		}

		size_t length;
		for(length = 0; fgets(buffer, sizeof(buffer), fp); length++) {
			if (buffer[0] == 'v')
				this->frVCnt++;
			else if (buffer[0] == 'i')
				this->frICnt++;
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
		this->frVertices = malloc(2 * sizeof(float) * this->frVCnt);
		this->frIndices = malloc(3 * sizeof(unsigned int) * this->frICnt);
		if (!this->frVertices || !this->frIndices) {
			#ifdef VERBOSE
				fputs("Fail to create buffer for focus region map vertices and/or indices\n", stderr);
			#endif
			fclose(fp);
			roadmap_destroy(this);
			return NULL;
		}

		float* v = this->frVertices;
		unsigned int* i = this->frIndices;
		for(size_t l = 0; l < length; l++) {
			if (fscanf(fp, "i %u %u %u\n", &i[0], &i[1], &i[2]) == 3) {
				i += 3;
			} else if (fscanf(fp, "v %f %f\n", &v[0], &v[1]) == 2) {
				v += 2;
			} else {
				#ifdef VERBOSE
					fprintf(stderr, "Error in focus region map file (phase-2: fetch) (line %zu)\n", l);
				#endif
				fclose(fp);
				roadmap_destroy(this);
				return NULL;
			}
		}

		fclose(fp);
		fp = NULL;
	}

	/* Project */ {
		fp = fopen(projectFile, "r");
		if (!fp) {
			#ifdef VERBOSE
				fprintf(stderr, "Fail to open project map file: %s (errno = %d)\n", projectFile, errno);
			#endif
			roadmap_destroy(this);
			return NULL;
		}

		size_t length;
		for(length = 0; fgets(buffer, sizeof(buffer), fp); length++) {
			if (buffer[0] == 'v')
				this->pVCnt++;
			else if (buffer[0] == 'i')
				this->pICnt++;
			else {
				#ifdef VERBOSE
					fprintf(stderr, "Error in project map file (phase-1: count) (line %zu)\n", length);
				#endif
				fclose(fp);
				roadmap_destroy(this);
				return NULL;
			}
		}

		rewind(fp);
		this->frVertices = malloc(4 * sizeof(float) * this->pVCnt);
		this->frIndices = malloc(3 * sizeof(unsigned int) * this->pICnt);
		if (!this->frVertices || !this->frIndices) {
			#ifdef VERBOSE
				fputs("Fail to create buffer for project map vertices and/or indices\n", stderr);
			#endif
			fclose(fp);
			roadmap_destroy(this);
			return NULL;
		}

		float* v = this->frVertices;
		unsigned int* i = this->frIndices;
		for(size_t l = 0; l < length; l++) {
			if (fscanf(fp, "v %f %f %f %f\n", &v[0], &v[1], &v[2], &v[3]) == 4) {
				v += 4;
			} else if (fscanf(fp, "i %u %u %u\n", &i[0], &i[1], &i[2]) == 3) {
				i += 3;
			} else {
				#ifdef VERBOSE
					fprintf(stderr, "Error in project map file (phase-2: fetch) (line %zu)\n", l);
				#endif
				fclose(fp);
				roadmap_destroy(this);
				return NULL;
			}
		}

		fclose(fp);
		fp = NULL;
	}

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

float* roadmap_getFocusRegionVertices(Roadmap this, size_t* size) {
	*size = this->frVCnt;
	return this->frVertices;
}

unsigned int* roadmap_getFocusRegionIndices(Roadmap this, size_t* size) {
	*size = this->frICnt;
	return this->frIndices;
}

float* roadmap_getProjectVertices(Roadmap this, size_t* size) {
	*size = this->pVCnt;
	return this->pVertices;
}

unsigned int* roadmap_getProjectIndices(Roadmap this, size_t* size) {
	*size = this->pICnt;
	return this->pIndices;
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
		fputs("Destroy roadmap class object\n", stdout);
		fflush(stdout);
	#endif

	if (this->frVertices)
		free(this->frVertices);
	if (this->frIndices)
		free(this->frIndices);
	if (this->pVertices)
		free(this->pVertices);
	if (this->pIndices)
		free(this->pIndices);
	if (this->geographic)
		free(this->geographic);

	free(this);
}