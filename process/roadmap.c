#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "roadmap.h"

roadmap roadmap_init(const char* const roadmapFile, char** const statue) {
	roadmap this = {
		.header = {
			.width = 0,
			.height = 0,
			.pCnt = 0,
			.fileSize = 0
		},
		.roadPoints = NULL,
		.t1 = NULL,
		.t2 = NULL
	};

	FILE* fp = fopen(roadmapFile, "rb");
	if (!fp) {
		*statue = "Fail to open roadmap file";
		return this;
	}
	if (!fread(&this.header, sizeof(this.header), 1, fp)) {
		*statue = "Error in roadmap file: Cannot read header";
		fclose(fp);
		return this;
	}

	unsigned int pixelCount = this.header.width * this.header.height;

	void* buffer = malloc( //Compact all data into one buffer
		pixelCount * sizeof(struct Roadmap_Table1) +
		pixelCount * sizeof(struct Roadmap_Table2) +
		this.header.pCnt * sizeof(struct RoadPoint)
	);
	if (!buffer) {
		*statue = "Fail to allocate buffer memory for roadmap data";
		fclose(fp);
		return this;
	}
	this.roadPoints = buffer;
	this.t1 = buffer +
		this.header.pCnt * sizeof(struct RoadPoint);
	this.t2 = buffer +
		this.header.pCnt * sizeof(struct RoadPoint) +
		pixelCount * sizeof(struct Roadmap_Table1);

	if (
		!fread(this.t1, sizeof(struct Roadmap_Table1), pixelCount, fp) ||
		!fread(this.t2, sizeof(struct Roadmap_Table2), pixelCount, fp)
	) {
		*statue = "Error in roadmap file: Cannot read tables";
		fclose(fp);
		free(buffer);
		this.roadPoints = NULL; //Invalid the data memory ptr
		return this;
	}
	for (struct RoadPoint* p = this.roadPoints; p < this.roadPoints + this.header.pCnt; p++) {
		float data[4]; //{Road-domain location (m) x, y, Screen domain position (0.0 for left and top, 1.0 for right and bottom) x, y}
		if (!fread(data, sizeof(data), 1, fp)) {
			*statue = "Error in roadmap file: Cannot read focus region";
			fclose(fp);
			free(buffer);
			this.roadPoints = NULL;
			return this;
		}
		*p = (struct RoadPoint){ .sx = data[2], .sy = data[3] };
	}

	*statue = NULL;
	fclose(fp);
	return this;
}

void roadmap_post(roadmap* this, float limitSingle, float limitMulti) {
	unsigned int width = this->header.width, height = this->header.height;
	for (unsigned int y = 0; y < height; y++) {
		for (unsigned int x = 0; x < width; x++) {
			unsigned int idx = y * width + x;
			float limit = x & 1 ? limitMulti : limitSingle; //Single frame limit on left pixel, multi frame limit on right pixel

			// Object max road-domain displacement (meter) in one frame
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

			// Inverse pixel width to avoid division: distance * 1/pixelWidth = number of pixels for that distance
			this->t1[idx].pw = 1 / this->t1[idx].pw;
		}
	}
}

void roadmap_destroy(roadmap* this) {
	if (!this->roadPoints)
		return;

	free(this->roadPoints);
	this->roadPoints = NULL;
}