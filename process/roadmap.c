#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef VERBOSE
#include <errno.h>
#endif

#include "roadmap.h"

struct Roadmap_ClassDataStructure {
	float* frVertices; //Focus region vertices {screen-x, screen-y}[frVCnt]
	size_t frVCnt;
	roadmap_header header;
	roadmap_t1* t1;
	roadmap_t2* t2;
};

Roadmap roadmap_init(const char* focusRegionFile, const char* roadmapFile, size2d_t size) {
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
		.frVertices = NULL,
		.t1 = NULL,
		.t2 = NULL
	};

	FILE* fp = NULL;
	char buffer[127];

	/* Map file */ {
		fp = fopen(roadmapFile, "rb");
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

		fclose(fp);
		fp = NULL;
	}

	/* Focus region */ {
		fp = fopen(focusRegionFile, "r");
		if (!fp) {
			#ifdef VERBOSE
				fprintf(stderr, "Fail to open focus region map file: %s (errno = %d)\n", focusRegionFile, errno);
			#endif
			roadmap_destroy(this);
			return NULL;
		}

		for(this->frVCnt = 0; fgets(buffer, sizeof(buffer), fp); this->frVCnt++);
		rewind(fp);

		this->frVertices = malloc(2 * sizeof(float) * this->frVCnt);
		if (!this->frVertices) {
			#ifdef VERBOSE
				fputs("Fail to create buffer for focus region vertices\n", stderr);
			#endif
			fclose(fp);
			roadmap_destroy(this);
			return NULL;
		}

		float* v = this->frVertices;
		for (size_t i = 0; i < this->frVCnt; i++) {
			if (fscanf(fp, "%f %f\n", &v[0], &v[1])) {
				v += 2;
			} else {
				#ifdef VERBOSE
					fprintf(stderr, "Error in focus region map file %s (line %zu)\n", focusRegionFile, i);
				#endif
				fclose(fp);
				roadmap_destroy(this);
				return NULL;
			}
		}

		fclose(fp);
		fp = NULL;
	}

	return this;
}

float* roadmap_getFocusRegion(Roadmap this, size_t* size) {
	*size = this->frVCnt;
	return this->frVertices;
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

void roadmap_destroy(Roadmap this) {
	if (!this)
		return;

	#ifdef VERBOSE
		fputs("Destroy roadmap class object\n", stdout);
		fflush(stdout);
	#endif

	if (this->frVertices)
		free(this->frVertices);
	if (this->t1)
		free(this->t1);
	if (this->t2)
		free(this->t2);

	free(this);
}