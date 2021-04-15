#include <stdlib.h>
#include <stdio.h>

#include "edge.h"

struct Edge_ClassDataStructure {
	luma_t* buffer;
	void* source;
	size_t width, height;
};

Edge edge_init(void* source, size_t width, size_t height) {
	Edge this = malloc(sizeof(struct Edge_ClassDataStructure));
	if (!this)
		return NULL;
	
	this->buffer = malloc(sizeof(luma_t) * (width-2) * (height-2));
	if (!(this->buffer))
		return NULL;

	this->source = source;
	this->width = width;
	this->height = height;
	return this;
}

void edge_process(Edge this) {
	size_t yLimit = this->height - 2, xLimit = this->width - 2; //Strip the edges
	luma_t* dest = this->buffer;

	for (size_t y = 0; y < yLimit; y++) {
		uint8_t* top = this->source + y * this->width * EDGE_SOURCEPIXELSIZE;
		uint8_t* middle = this->source + (y+1) * this->width * EDGE_SOURCEPIXELSIZE;
		uint8_t* bottom = this->source + (y+2) * this->width * EDGE_SOURCEPIXELSIZE;
		
		for (size_t x = 0; x < xLimit; x++) {
			int16_t luma = 0; //luma of edge, +/- 32k is large enough for any filter mask
//			luma += -1 * (top[0] + top[1] + top[2]); //top-left
			luma += -1 * (top[EDGE_SOURCEPIXELSIZE + 0] + top[EDGE_SOURCEPIXELSIZE + 1] + top[EDGE_SOURCEPIXELSIZE + 2]); //top-center
//			luma += -1 * (top[EDGE_SOURCEPIXELSIZE*2 + 0] + top[EDGE_SOURCEPIXELSIZE*2 + 1] + top[EDGE_SOURCEPIXELSIZE*2 + 2]); //top-right
			luma += -1 * (middle[0] + middle[1] + middle[2]); //middle-top
			luma += +4 * (middle[EDGE_SOURCEPIXELSIZE + 0] + middle[EDGE_SOURCEPIXELSIZE + 1] + middle[EDGE_SOURCEPIXELSIZE + 2]); //middle-center
			luma += -1 * (middle[EDGE_SOURCEPIXELSIZE*2 + 0] + middle[EDGE_SOURCEPIXELSIZE*2 + 1] + middle[EDGE_SOURCEPIXELSIZE*2 + 2]); //middle-right
//			luma += -1 * (bottom[0] + bottom[1] + bottom[2]); //bottom-left
			luma += -1 * (bottom[EDGE_SOURCEPIXELSIZE + 0] + bottom[EDGE_SOURCEPIXELSIZE + 1] + bottom[EDGE_SOURCEPIXELSIZE + 2]); //bottom-center
//			luma += -1 * (bottom[EDGE_SOURCEPIXELSIZE*2 + 0] + bottom[EDGE_SOURCEPIXELSIZE*2 + 1] + bottom[EDGE_SOURCEPIXELSIZE*2 + 2]); //bottom-right
			top += EDGE_SOURCEPIXELSIZE;
			middle += EDGE_SOURCEPIXELSIZE;
			bottom += EDGE_SOURCEPIXELSIZE;
			/* Range of luma is +/- 255 * 3 * middle-center coff = 3060 */
			if (luma > 255)
				*(dest++) = 255;
			else if (luma < 0)
				*(dest++) = 0;
			else
				*(dest++) = luma;
			//*(dest++) = abs(luma>>4); //Skip LSB to minimize noise
		}
	}
}

luma_t* edge_getEdgeImage(Edge this) {
	return this->buffer;
}

void edge_destroy(Edge this) {
	free(this->buffer);
	free(this);
}