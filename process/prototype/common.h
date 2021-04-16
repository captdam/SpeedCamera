/** Common config and define 
 */

#ifndef INCLUDE_COMMON_H
#define INCLUDE_COMMON_H

#include <inttypes.h>

/** Size of each pixel of the source image 
 * 1: monochrome, grayscale 
 * 2: RGB565, not implemented yet 
 * 3: RGB, 8-bit each channel 
 * 4: RGBA, 8-bit each channel 
 */
#define SOURCEFILEPIXELSZIE 3

/** Internal data type used to store luma value of the edge filtered image
 */
typedef uint8_t luma_t;

/** Size of 2D object (image): width and height in pixel
 */
typedef struct Size2D_t {
	size_t width;
	size_t height;
} size2d_t;

#endif /* #ifndef INCLUDE_COMMON_H */