/** Class - Source.class. 
 * Open, read and close plain uncompressed plain bitmap video file. 
 */

#ifndef INCLUDE_SOURCE_H
#define INCLUDE_SOURCE_H

#include <inttypes.h>

#include "common.h"

/** Source class object data structure
 */
typedef struct Source_ClassDataStructure* Source;

/** Init a source object. 
 * Open the video file (source file) and create a buffer for reading frame. 
 * @param imageFile Name of the video file
 * @param resolution Size of camera, image resolution
 * @param bytePerPixel Bytes per pixel, 1 (mono gray), 2 (RGB565), 3 (RGB) or 4 (RGBA)
 * @return $this(Opaque) source class object if success, null if fail
 */
Source source_init(const char* imageFile, size2d_t resolution, size_t bytePerPixel);

/** Read one frame from the source file and save in buffer of this class object. 
 * Use source_getRawBitmap() to get pointer of the buffer. 
 * @param this This source class object
 * @return Number of bytes fetched from source file
 */
size_t source_read(Source this);

/** Get the pointer to buffer. 
 * Here is where the raw frame saved. This address will not change. 
 * @param this This source class object
 * @return A pointer to the raw frame data saved in this object's buffer
 */
void* source_getRawBitmap(Source this);

/** Destroy this source class object. 
 * @param this This source class object
 */
void source_destroy(Source this);

#endif /* #ifndef INCLUDE_SOURCE_H */