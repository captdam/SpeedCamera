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
 * Init a new joinable thread to read frame data from file, FIFO or Pi Camera. The first letter indicates the type. 
 * To use Pi Camera, pass: "c" (c means camera); 
 * To use FIFO file, pass: "f" (f means FIFO); 
 * To use real file, pass: "dir/to/file" (if the argument is neither "c" or "f", the program read it as filepath). 
 * A temp FIFO file call "tempframefifo.data" at current work directory will be created if using camera or fifo. 
 * Init a double buffer object to sharing frame data between main thread and this thread. 
 * In case of FIFO, this function will block until the writer program writes the video file header to the FIFO. 
 * This function will create a new thread upon success. 
 * Double buffer: This thread will read and pre-process frame n+1 while main threa processing frame n. 
 * @param filename Path to the FIFO
 * @return $this(Opaque) MT-Source class object upon success. If fail, free all resource and return NULL
 */
MT_Source mt_source_init(const char* filename);

/** Get the video info of video. 
 * @param this This MT-Source class object
 * @return Video header data sturcture
 */
vh_t mt_source_getInfo(MT_Source this);

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