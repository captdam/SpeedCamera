/** Class - edge.class. 
 * A kernel filter used to detect edge in image. 
 */

#ifndef INCLUDE_EDGE_H
#define INCLUDE_EDGE_H

#include <inttypes.h>

#include "common.h"

/** Edge class object data structure
 */
typedef struct Edge_ClassDataStructure* Edge;

/** Init a edge object. 
 * This filter has an input, which is a frame of uncompressed video (buffer of source.class); 
 * and an output buffer build into this object used to store the result. 
 * @param source Pointer to source object's buffer
 * @param resolution Size of camera, image resolution
 * @param bytePerPixel Bytes per pixel, 1 (mono gray), 2 (RGB565), 3 (RGB) or 4 (RGBA)
 * @return $this(Opaque) edge class object if success, null if fail
 */
Edge edge_init(void* source, size2d_t resolution, size_t bytePerPixel);

/** Apply edge detection filter and save the result in the buffer. 
 * Use edge_getEdgeImage() to get pointer of the buffer. 
 * @param this This edge class object
 */
void edge_process(Edge this);

/** Get the size of the buffer. 
 * Here is size of the edge filtered image.
 * @param this This edge class object
 * @return Size of the buffer in pixel
 */
size2d_t edge_getEdgeSize(Edge this);

/** Get the pointer to buffer. 
 * Here is where the edge filtered image saved. This address will not change. 
 * @param this This edge class object
 * @return A pointer to the filtered image saved in this object's buffer
 */
luma_t* edge_getEdgeImage(Edge this);

/** Destroy this edge class object. 
 * @param this This edge class object
 */
void edge_destroy(Edge this);

#endif /* #ifndef INCLUDE_EDGE_H */