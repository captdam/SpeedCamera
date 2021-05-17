#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <stdio.h>

#include "compare.h"

#define LUMA_THRESHOLD 40
#define DISTANCE_THRESHOLD 100
#define SEARCH_REGION 10

struct Compare_ClassDataStructure {
	size2d_t size;
	float ratio;
	luma_t* projectSource;
	luma_t* projectBuffer;
	uint8_t* speed;
	loc3d_t* location;
};

Compare compare_init(luma_t* input, size2d_t size, float maxSpeed, float fps) {
	Compare this = malloc(sizeof(struct Compare_ClassDataStructure));
	if (!this)
		return NULL;
	
	this->size = size;
	this->ratio = 255 / (maxSpeed / 3.6 / fps);
	this->projectSource = input;
	this->projectBuffer = NULL;
	this->speed = NULL;
	this->location = NULL;
	
	size_t area = size.width * size.height;
	this->projectBuffer = malloc(area * sizeof(luma_t));
	if (!(this->projectBuffer)) {
		compare_destroy(this);
		return NULL;
	}
	this->speed = calloc(area, sizeof(uint8_t));
	if (!(this->speed)) {
		compare_destroy(this);
		return NULL;
	}
	this->location = malloc(area * sizeof(loc3d_t));
	if (!(this->location)) {
		compare_destroy(this);
		return NULL;
	}
	
	return this;
}

void compare_setLocationMap(Compare this, loc3d_t* location) {
	memcpy(this->location, location, sizeof(loc3d_t) * this->size.width * this->size.height);
}

float distanceLoc3d(loc3d_t a, loc3d_t b) {
	return sqrt( powf(a.x-b.x,2) + powf(a.y-b.y,2) + powf(a.z-b.z,2) );
}
uint8_t clamp(float i) {
	return (i > 255) ? 255 : i;
}
void compare_process(Compare this) {
	memset(this->speed, 0, sizeof(uint8_t) * this->size.width * this->size.height);

	for (size_t y = 0; y < this->size.height; y++) {
		for (size_t x = 0; x < this->size.width; x++) {
			size_t indexSelf = y * this->size.width + x;
			if (this->projectSource[indexSelf] > LUMA_THRESHOLD) {
				float min = 999999999.9f;
				size_t l = x > SEARCH_REGION ? x - SEARCH_REGION : 0;
				size_t r = x < this->size.width - SEARCH_REGION ? x + SEARCH_REGION : this->size.width;
				size_t u = y > SEARCH_REGION ? y - SEARCH_REGION : 0;
				size_t d = y < this->size.height - SEARCH_REGION ? y + SEARCH_REGION : this->size.height;
				for (size_t sy = u; sy < d; sy++) {
					for (size_t sx = l; sx < r; sx++) {
						if (abs(sx-x) < 2 || abs(sy-y) < 2)
							continue;
						size_t indexSearch = sy * this->size.width + sx;
						if (this->projectBuffer[indexSearch] > LUMA_THRESHOLD) {
							float d = distanceLoc3d(this->location[indexSelf], this->location[indexSearch]);
							if (d < min)
								min = d;
						}
					}
				}
				this->speed[indexSelf] = clamp( this->ratio * d );
			}
/*			size_t indexSelf = y * this->size.width + x;
			if (this->projectSource[indexSelf] > LUMA_THRESHOLD) {
				for (size_t d = 0; d < DISTANCE_THRESHOLD; d++) {
					size_t indexLeft = indexSelf - d;
					size_t indexRight = indexSelf + d;
					size_t indexUp = indexSelf - d * this->size.width;
					size_t indexDown = indexSelf + d * this->size.width;
					if (this->projectBuffer[indexLeft] > LUMA_THRESHOLD) {
						if (d > 1) //Noise when distance = 1
							this->speed[indexSelf] = clamp( this->ratio * distanceLoc3d(this->location[indexSelf], this->location[indexLeft]) );
						break;
					}
					if (this->projectBuffer[indexRight] > LUMA_THRESHOLD) {
						if (d > 1)
							this->speed[indexSelf] = clamp( this->ratio * distanceLoc3d(this->location[indexSelf], this->location[indexRight]) );
						break;
					}
					if (this->projectBuffer[indexUp] > LUMA_THRESHOLD) {
						if (d > 1)
							this->speed[indexSelf] = clamp( this->ratio * distanceLoc3d(this->location[indexSelf], this->location[indexUp]) );
						break;
					}
					if (this->projectBuffer[indexDown] > LUMA_THRESHOLD) {
						if (d > 1)
							this->speed[indexSelf] = clamp( this->ratio * distanceLoc3d(this->location[indexSelf], this->location[indexDown]) );
						break;
					}
				}
			}*/
		}
	}

	memcpy(this->projectBuffer, this->projectSource, sizeof(luma_t) * this->size.width * this->size.height);
}

size2d_t compare_getMapSize(Compare this) {
	return this->size;
}

uint8_t* compare_getSpeedMap(Compare this) {
	return this->speed;
}

void compare_destroy(Compare this) {
	if (!this)
		return;
	
	free(this->projectBuffer);
	free(this->speed);
	free(this->location);
	free(this);
}

#undef LUMA_THRESHOLD
#undef DISTANCE_THRESHOLD