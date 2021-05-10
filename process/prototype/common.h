/** Common config and define 
 */

#ifndef INCLUDE_COMMON_H
#define INCLUDE_COMMON_H

#include <inttypes.h>

/** Internal data type used to store luma value of the edge filtered image 
 */
typedef uint8_t luma_t;

/** Size of 2D object (image): width and height in pixel 
 */
typedef struct Size2D_t {
	size_t width;
	size_t height;
} size2d_t;

/** Describe a real-world location 
 */
typedef struct Location3D_t {
	float x;
	float y;
	float z;
} loc3d_t;

#endif /* #ifndef INCLUDE_COMMON_H */