/** Class - edge.class. 
 * A kernel filter used to detect edge in image. 
 */

#ifndef INCLUDE_EDGE_H
#define INCLUDE_EDGE_H

#include <inttypes.h>

#include "common.h"

#define EDGE_SOURCEPIXELSIZE SOURCEFILEPIXELSZIE

/** Edge class object data structure
 */
typedef struct Edge_ClassDataStructure* Edge;

/** Init a edge object. 
 * This filter has an input (buffer of sourcfe.class), with same width and height of this object; 
 * and a output buffer build into this object used to store the result. 
 * Edge of the input is stripped, so the size of the result is (width-2) * (height-2). 
 * @param source Pointer to source object's buffer
 * @param width Width of the input image
 * @param height Height of the input image
 * @return $this(Opaque) edge class object if success, null if fail
 */
Edge edge_init(void* source, size_t width, size_t height);

/** Apply edge detection filter and save the result in the buffer. 
 * Use edge_getEdgeImage() to get pointer of the buffer. 
 * @param this This edge class object
 */
void edge_process(Edge this);

/** Get the pointer to buffer. 
 * Here is where the edge filtered image saved.
 * @param this This edge class object
 * @return A pointer to the filtered image saved in this object's buffer
 */
luma_t* edge_getEdgeImage(Edge this);

/** Destroy this edge class object. 
 * @param this This edge class object
 */
void edge_destroy(Edge this);

#endif /* #ifndef INCLUDE_EDGE_H */