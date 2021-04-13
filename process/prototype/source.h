/** Class - Source.class. 
 * Open, read and close plain uncompressed plain bitmap video file. 
 */

#ifndef INCLUDE_SOURCE_H
#define INCLUDE_SOURCE_H

#include <inttypes.h>

#include "common.h"

#define SOURCE_PIXELSIZE SOURCEFILEPIXELSZIE

/** Source class object data structure
 */
typedef struct Source_ClassDataStructure Source;

/** Init a source object. 
 * Open the video file (source file) and create a buffer for reading frame. 
 * Use source_initCheck() to verify the result of this call. 
 * @param imageFile Name of the video file
 * @param width Width of the image
 * @param height Height of the image
 * @return $this(Opaque) source class object if success, null if fail
 */
Source* source_init(const char* imageFile, size_t width, size_t height);

/** Read one frame from the source file. 
 * Raw frame saved in buffer of this class object. 
 * Use source_getRawBitmap() to fetch the result. 
 * @param this This source class object
 * @return Number of bytes processed
 */
size_t source_read(Source* this);

/** Get the pointer to buffer. 
 * Here is where the raw frame saved.
 * @param this This source class object
 * @return A pointer to the raw frame data saved in this object's buffer
 */
void* source_getRawBitmap(Source* this);

/** Destroy this source class object. 
 * @param this This source class object
 */
void source_destroy(Source* this);

#endif /* #ifndef INCLUDE_SOURCE_H */