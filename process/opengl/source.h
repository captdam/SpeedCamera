/** Class - Source.class. 
 * Open, read and close plain uncompressed plain bitmap video file. 
 */

#ifndef INCLUDE_SOURCE_H
#define INCLUDE_SOURCE_H

#include "common.h"
#include "gl.h"

/** Source class object data structure
 */
typedef struct Source_ClassDataStructure* Source;

/** Init a source object, open the video file (source file), and allocate internal buffer. 
 * @param imageFile Name of the video file
 * @param info Video file info: resolution, fps and color scheme, pass-by-reference
 * @return $this(Opaque) source class object if success, null if fail
 */
Source source_init(const char* videoFile, vh_t* info);

/** Read a frame from the source file into internal buffer. 
 * @param this This source class object
 * @return Number of bytes fetched from source file
 */
size_t source_read(Source this);

/** Get the content just fetched from the source file (pointer to the internal buffer). 
 * This pointer in fact will never change. 
 * @param this This source class object
 * @return Pointer to the content
 */
char* source_get(Source this);

/** Destroy this source class object, close the source file. 
 * @param this This source class object
 */
void source_destroy(Source this);

#endif /* #ifndef INCLUDE_SOURCE_H */