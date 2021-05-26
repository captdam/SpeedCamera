#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <stdio.h>

#include "compare.h"

#define LUMA_THRESHOLD 40

typedef struct Compare_Neighbor {
	uint8_t distance;
	size_t pos;
} neighbor;

typedef struct Compare_selfLocation {
	size_t neighborCount;
	neighbor* neighbor;
} selfLocation;

struct Compare_ClassDataStructure {
	size2d_t size;
	uint8_t* previousFrame;
	selfLocation* locationMap;
};

float distanceLoc3d(loc3d_t a, loc3d_t b) {
	return sqrt( powf(a.x-b.x,2) + powf(a.y-b.y,2) + powf(a.z-b.z,2) );
}

Compare compare_init(size2d_t size, loc3d_t* positionMap, float distanceThreshold) {
	Compare this = malloc(sizeof(struct Compare_ClassDataStructure));
	if (!this)
		return NULL;
	
	this->size = size;
	this->previousFrame = NULL;
	this->locationMap = NULL;

	size_t area = size.width * size.height;

	this->previousFrame = malloc(area * sizeof(*this->previousFrame)); 
	if (!this->previousFrame) {
		compare_destroy(this);
		return NULL;
	}

	this->locationMap = calloc(area, sizeof(*this->locationMap)); //Init this->locationMap.neighbor[] to NULL pointer
	if (!this->locationMap) {
		compare_destroy(this);
		return NULL;
	}

	size_t i = 0;
	for (size_t y = 0; y < size.height; y++) {
		for (size_t x = 0; x < size.width; x++) {
			loc3d_t selfPos = positionMap[i];
//			printf("Pixel (%04zu,%04zu) pos (%f,%f,%f)\n", x, y, selfPos.x, selfPos.y, selfPos.z);

			//Get search window and count number of near neighbor
			size_t xl = x, xr = x, yu = y, yd = y;
			size_t count = 0;
			while (1) { //Stop if reach edge or next step will excess threshold
				if (xl == 0) //On edge
					break;
				size_t j = y * size.width + xl - 1;
				if (distanceLoc3d(selfPos, positionMap[j]) > distanceThreshold) //Excess threshold
					break;
				xl--;
			}
			count += x - xl;
			while (1) {
				if (xr == size.width - 1)
					break;
				size_t j = y * size.width + xr + 1;
				if (distanceLoc3d(selfPos, positionMap[j]) > distanceThreshold)
					break;
				xr++;
			}
			count += xr - x;
			while (1) {
				if (yu == 0)
					break;
				size_t j = (yu - 1) * size.width + x;
				if (distanceLoc3d(selfPos, positionMap[j]) > distanceThreshold)
					break;
				yu--;
			}
			count += y - yu;
			while (1) {
				if (yd == size.height - 1)
					break;
				size_t j = (yd + 1) * size.width + x;
				if (distanceLoc3d(selfPos, positionMap[j]) > distanceThreshold)
					break;
				yd++;
			}
			count += yd - y;
//			printf("Pixel (%04zu,%04zu) search region x=[%04zu,%04zu], y=[%04zu,%04zu], has %zu candidates\n", x, y, xl, xr, yu, yd, count);

			//Create candidate array
			this->locationMap[i].neighborCount = count;
			this->locationMap[i].neighbor = malloc(count * sizeof(neighbor));
			size_t k = 0;
			for (size_t cx = xl; cx <= xr; cx++) {
				if (cx == x) continue; //Do not include self
				size_t j = y * size.width + cx;
				this->locationMap[i].neighbor[k++] = (neighbor){
					.distance = distanceLoc3d(selfPos, positionMap[j]) / distanceThreshold * 0xFF,
					.pos = y * size.width + cx
				};
			}
			for (size_t cy = yu; cy <= yd; cy++) {
				if (cy == y) continue; //Do not include self
				size_t j = cy * size.width + x;
				this->locationMap[i].neighbor[k++] = (neighbor){
					.distance = distanceLoc3d(selfPos, positionMap[j]) / distanceThreshold * 0xFF,
					.pos = cy * size.width + x
				};
			}
/*			for (size_t cy = yu; cy <= yd; cy++) {
				for (size_t cx = xl; cx <= xr; cx++) {
					if (cy == y && cx == x) continue; //Do not include self
					size_t j = cy * size.width + cx;
					float distance = distanceLoc3d(selfPos, positionMap[j]);
					if (distance < distanceThreshold) {
						this->locationMap[i].neighbor[k] = (neighbor){
							.distance = distance,
							.pos = (size2d_t) {.width = cx, .height = cy}
						};
					}
				}
			}*/

			//Sort candidate array
			if (count > 1) {
				for (size_t j = 0; j < count - 1; j++) {
					for (size_t k = j + 1; k < count; k++) {
						if (this->locationMap[i].neighbor[k].distance < this->locationMap[i].neighbor[j].distance) {
							neighbor temp = this->locationMap[i].neighbor[k];
							this->locationMap[i].neighbor[j] = this->locationMap[i].neighbor[k];
							this->locationMap[i].neighbor[k] = temp;
						}
					}
				}
			}

			i++;
		}
	}


	return this;
}

void compare_process(Compare this, uint8_t* dest, uint8_t* src) {
	size_t area = this->size.height * this->size.width;
	memset(dest, 0, area * sizeof(*dest));

	for (size_t i = area; i; i--) {
//		dest[i] = this->locationMap[i].neighborCount;
		if (src[i] > LUMA_THRESHOLD && this->previousFrame[i] < LUMA_THRESHOLD) {
			selfLocation currentSource = this->locationMap[i];
			for (size_t j = 0; j < currentSource.neighborCount; j++) {
				neighbor currentTarget = currentSource.neighbor[j];
				if ( this->previousFrame[ currentTarget.pos ] > LUMA_THRESHOLD ) {
					dest[i] = currentTarget.distance;
					break;
				}
			}
		}
	}
	
	memcpy(this->previousFrame, src, area * sizeof(*src));
}

void compare_destroy(Compare this) {
	if (!this)
		return;
	
	free(this->previousFrame);
	
	if (this->locationMap) {
		for (size_t i = this->size.width * this->size.height; i; i--) {
			free(this->locationMap[i].neighbor);
		}
	}
	free(this->locationMap);

	free(this);
}