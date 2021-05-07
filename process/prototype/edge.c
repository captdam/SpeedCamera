/* Edge of the input is stripped, so the size of the result is (width-2) * (height-2). */

#define WEIGHT_TL -1
#define WEIGHT_TC -1
#define WEIGHT_TR -1
#define WEIGHT_ML -1
#define WEIGHT_MC +8
#define WEIGHT_MR -1
#define WEIGHT_BL -1
#define WEIGHT_BC -1
#define WEIGHT_BR -1
#define WEIGHT_TOTAL abs(WEIGHT_TL)+abs(WEIGHT_TC)+abs(WEIGHT_TR)+abs(WEIGHT_ML)+abs(WEIGHT_MC)+abs(WEIGHT_MR)+abs(WEIGHT_BL)+abs(WEIGHT_BC)+abs(WEIGHT_BR)

#include <stdlib.h>

#include "edge.h"

struct Edge_ClassDataStructure {
	void* source;
	luma_t* buffer;
	size_t bytePerPixel;
	size2d_t cameraSize, edgeSize;
};

Edge edge_init(void* source, size2d_t resolution, size_t bytePerPixel) {
	Edge this = malloc(sizeof(struct Edge_ClassDataStructure));
	if (!this)
		return NULL;
	
	this->source = source;
	this->buffer = NULL;
	this->bytePerPixel = bytePerPixel;
	this->cameraSize = resolution;
	this->edgeSize = (size2d_t){.width = resolution.width - 2, .height = resolution.height - 2}; //Strip the edges

	this->buffer = malloc(sizeof(luma_t) * this->edgeSize.width * this->edgeSize.height);
	if (!(this->buffer)) {
		edge_destroy(this);
		return NULL;
	}
	
	return this;
}

void edge_process(Edge this) {
	luma_t* dest = this->buffer;
	for (size_t y = 0; y < this->edgeSize.height; y++) {
		uint8_t* top = this->source + y * this->cameraSize.width * this->bytePerPixel;
		uint8_t* middle = this->source + (y+1) * this->cameraSize.width * this->bytePerPixel;
		uint8_t* bottom = this->source + (y+2) * this->cameraSize.width * this->bytePerPixel;
		
		for (size_t x = 0; x < this->edgeSize.width; x++) {
			int16_t luma = 0; //luma of edge, +/- 32k is large enough for any filter mask
#if WEIGHT_TL != 0 //top-left
			luma += WEIGHT_TL * (top[0] + top[1] + top[2]);
#endif
#if WEIGHT_TC != 0 //top-center
			luma += WEIGHT_TC * (top[this->bytePerPixel + 0] + top[this->bytePerPixel + 1] + top[this->bytePerPixel + 2]);
#endif
#if WEIGHT_TR != 0 //top-right
			luma += WEIGHT_TR * (top[this->bytePerPixel*2 + 0] + top[this->bytePerPixel*2 + 1] + top[this->bytePerPixel*2 + 2]);
#endif
#if WEIGHT_ML != 0 //middle-top
			luma += WEIGHT_ML * (middle[0] + middle[1] + middle[2]);
#endif
#if WEIGHT_MC != 0 //middle-center
			luma += WEIGHT_MC * (middle[this->bytePerPixel + 0] + middle[this->bytePerPixel + 1] + middle[this->bytePerPixel + 2]);
#endif
#if WEIGHT_MR != 0 //middle-right
			luma += WEIGHT_MR * (middle[this->bytePerPixel*2 + 0] + middle[this->bytePerPixel*2 + 1] + middle[this->bytePerPixel*2 + 2]);
#endif
#if WEIGHT_BL != 0 //bottom-left
			luma += WEIGHT_BL * (bottom[0] + bottom[1] + bottom[2]);
#endif
#if WEIGHT_BC != 0 //bottom-center
			luma += WEIGHT_BC * (bottom[this->bytePerPixel + 0] + bottom[this->bytePerPixel + 1] + bottom[this->bytePerPixel + 2]);
#endif
#if WEIGHT_BR != 0 //bottom-right
			luma += WEIGHT_BR * (bottom[this->bytePerPixel*2 + 0] + bottom[this->bytePerPixel*2 + 1] + bottom[this->bytePerPixel*2 + 2]);
#endif
			top += this->bytePerPixel;
			middle += this->bytePerPixel;
			bottom += this->bytePerPixel;
			luma /= WEIGHT_TOTAL;
			*(dest++) = luma < 0 ? 0 : luma;
		}
	}
}

size2d_t edge_getEdgeSize(Edge this) {
	return this->edgeSize;
}

luma_t* edge_getEdgeImage(Edge this) {
	return this->buffer;
}

void edge_destroy(Edge this) {
	if (!this)
		return;

	free(this->buffer);
	free(this);
}

#undef WEIGHT_TL
#undef WEIGHT_TC
#undef WEIGHT_TR
#undef WEIGHT_ML
#undef WEIGHT_MC
#undef WEIGHT_MR
#undef WEIGHT_BL
#undef WEIGHT_BC
#undef WEIGHT_BR
#undef WEIGHT_TOTAL