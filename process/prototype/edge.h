/** Class - edge.class. 
 * A kernel filter used to detect edge in image. 
 */

#ifndef INCLUDE_EDGE_H
#define INCLUDE_EDGE_H

#include <inttypes.h>

#include "common.h"

#define EDGE_SOURCEPIXELSIZE SOURCEFILEPIXELSZIE

/** Apply edge detection filter and save the result. 
 * Edge is stripped, so the dest buffer is of size (width-2) * (height-2). 
 * @param source Pointer to raw frame data
 * @param dest Pointer to where to write the filtered image
 * @param width Width of the image
 * @param height Height of the image
 */
void edge(void* source, luma_t* dest, size_t width, size_t height);

#endif /* #ifndef INCLUDE_EDGE_H */