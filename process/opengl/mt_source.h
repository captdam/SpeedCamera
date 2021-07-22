/** Class - [MultiThread] Source.class. 
 * Read frame data from FIFO. 
 * A FIFO is used to transmit frame data from camera process to this process. 
 * A double buffer is used to sharing data between main thread and this thread. 
 */

#ifndef INCLUDE_MT_SOURCE_H
#define INCLUDE_MT_SOURCE_H

#include "common.h"

/** MT_Source class object data structure
 */
typedef struct MultiThread_Source_ClassDataStructure* MT_Source;

/** Create a MT_Source class. 
 * Init the inter-process communication FIFO with camera program. Get the video frame info. 
 * Init a new joinable thread to read frame data from FIFO. 
 * Init a double buffer object to sharing frame data between main thread and this thread. 
 * This function will block until the camera program writes the video file header to the FIFO. 
 * This function will create a new thread upon success. 
 * Double buffer: This thread will read frame n+1 while main threa processing frame n. 
 * @param filename Path to the FIFO
 * @return $this(Opaque) MT-Source class object upon success. If fail, free all resource and return NULL
 */
MT_Source mt_source_init(const char* filename);

/** Get the size of video. 
 * @param this This MT-Source class object
 * @return Size of frame data in pixel
 */
size2d_t mt_source_getSize(MT_Source this);

/** Poll a new frame from the source, get the pointer to the data. 
 * This function will block until the frame data is ready in the double buffer. 
 * Call this before using the data, call mt_source_finish() after the data no longer required to release the buffer. 
 * Data remain unchanged before call mt_source_finish(). 
 * @param this This MT-Source class object
 * @return A pointer to the ready-to-use frame data. In case of broken pipe or EOF, return NULL (user's resbosibility to destroy the class)
 */
void* mt_source_start(MT_Source this);

/** Finish with current frame.
 * Release the current using double buffer, so the reader can use it to load next frame. 
 * Call this after GPU DMA is done (gl_finish())
 * @param this This MT-Source class object
 */
void mt_source_finish(MT_Source this);

/** Destory this MT-Source class, free all resources and cancel the thread. 
 * @param this This MT-Source class object
 */
void mt_source_destroy(MT_Source this);

#endif /* #ifndef INCLUDE_MT_SOURCE_H */