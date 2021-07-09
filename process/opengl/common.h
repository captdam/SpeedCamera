/** Common config and define 
 */

#ifndef INCLUDE_COMMON_H
#define INCLUDE_COMMON_H

#include <inttypes.h>

/** Video file header
 */
typedef struct VideoHeader_t {
	uint16_t width;
	uint16_t height;
	uint16_t fps;
	uint16_t colorScheme;
} vh_t;

/** Size of 2D object (image): width and height in pixel 
 */
typedef struct Size2D_t {
	union {
		size_t width;
		size_t x;
	};
	union {
		size_t height;
		size_t y;
	};
} size2d_t;

/** Describe a real-world location 
 */
typedef struct Location3D_t {
	float x;
	float y;
	float z;
} loc3d_t;

/** Neighbor point of a road point
 */
typedef struct RoadNeighbor {
	unsigned int distance: 8;
	unsigned int pos: 24;
} neighbor_t;
#define ROADMAP_DIS_MAX 255
#define ROADMAP_POS_MAX 16777215

/** A road point
 */
typedef struct RoadPoint {
	neighbor_t* neighborStart;
	neighbor_t* neighborEnd;
} road_t;

#endif /* #ifndef INCLUDE_COMMON_H */