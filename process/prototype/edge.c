#include <stdlib.h>
#include <stdio.h>

#include "edge.h"

void edge(void* source, luma_t* dest, size_t width, size_t height) {
//	uint16_t* destPtr = dest; //Size of (width-2) * (height-2), edge stripped
	for (size_t y = 0; y < height - 2; y++) { //Skip the edge
		uint8_t* top = source + y * width * EDGE_SOURCEPIXELSIZE;
		uint8_t* middle = source + (y+1) * width * EDGE_SOURCEPIXELSIZE;
		uint8_t* bottom = source + (y+2) * width * EDGE_SOURCEPIXELSIZE;
		
		for (size_t x = 0; x < width - 2; x++) {
			int16_t luma = 0; //luma of edge, +/-32k is large enough for worst case
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
			*(dest++) = abs(luma>>2); //Skip LSB to minimize noise
		}
	}
}