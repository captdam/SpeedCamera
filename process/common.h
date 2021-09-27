/** Common config and define 
 */

#ifndef INCLUDE_COMMON_H
#define INCLUDE_COMMON_H

#include <inttypes.h>

/** Video file header
 */
typedef struct VideoHeader_t {
	uint16_t width;
	uint16_t height;
	uint16_t fps;
	uint16_t colorScheme;
} vh_t;

/** Size of 2D object (image): width and height in pixel 
 */
typedef struct Size2D_t {
	union {
		size_t width;
		size_t x;
	};
	union {
		size_t height;
		size_t y;
	};
} size2d_t;

/** Size of 3D object (image): width, height and depth
 */
typedef struct Size3D_t {
	union {
		size_t width;
		size_t x;
	};
	union {
		size_t height;
		size_t y;
	};
	union {
		size_t depth;
		size_t z;
	};
} size3d_t;

/** Describe a real-world location 
 */
typedef struct Location3D_t {
	float x;
	float y;
	float z;
} loc3d_t;

/** Neighbor point of a road point
 */
typedef struct RoadNeighbor {
	unsigned int distance: 8;
	unsigned int pos: 24;
} neighbor_t;
#define ROADMAP_DIS_MAX 255
#define ROADMAP_POS_MAX 16777215

/** A road point
 */
typedef struct RoadPoint {
	neighbor_t* neighborStart;
	neighbor_t* neighborEnd;
} road_t;

/** Get the current machine time in nanosecond
 * @return Current time
 */
uint64_t nanotime();

/* == Fifo2 == Double buffer ================================================================ */

/** Double buffer FIFO util (Fifo2), used for sending data between threads. 
 * A simple, light weigth, multi-thread 2-stage FIFO, without using system pipe or fifo. 
 * Note: This util uses mailbox instead of mutex, make sure one and only one thread is reading; one and only one thread is writing. 
 */
typedef volatile struct Util_DoubleBufferFIFO* Fifo2;

/** Creating a new Fifo2 class object. 
 * @param size Size of the buffer
 * @return $this(Opaque) Fifo2 class object upon success. If fail, free all resource and return NULL
 */
Fifo2 fifo2_init(size_t size);

/** Get the Fifo2 object status. 
 * @param this This Fifo2 class object
 * @param size Pass-by-reference: size of a single buffer inside the Fifo2
 * @param space Pass-by-reference: remaining space in the buffer. 0 = full, 2 = empty
 */
void fifo2_status(Fifo2 this, size_t* size, unsigned int* space);

/** Write data into Fifo2. 
 * If param size is non-zero, size bytes of data will be read from the src and write to internal buffer; 
 * If param size is 0, the length of data transfer is defined by the size of internal buffer. 
 * If param size > internal buffer size, program may crash; 
 * If param size < internal buffer size, remaining space remain unchanged. 
 * @param this This Fifo2 class object
 * @param src Pointer to the source of data
 * @param size Size of data, see description
 * @return 1 if success; 0 is fail (buffer full)
 */
int fifo2_write(Fifo2 this, void* src, size_t size);

/** Get the write side pointer to manually write to the Fifo2. 
 * This function will block until there is free internal buffer. 
 * Call fifo2_write_finish() to finish the writing so the read side can use it. 
 * @param this This Fifo2 class object
 * @return Write side pointer
 */
void* fifo2_write_start(Fifo2 this);

/** Finish the manual write to the Fifo2. 
 * Use with fifo2_write_start(). 
 * @param this This Fifo2 class object
 */
void fifo2_write_finish(Fifo2 this);

/** Read data from Fifo2. 
 * If param size is non-zero, size bytes of data will be read from the internal buffer and write to dest; 
 * If param size is 0, the length of data transfer is defined by the size of internal buffer. 
 * If param size > internal buffer size, program may crash; 
 * If param size < internal buffer size, remaining space will be discarded. 
 * @param this This Fifo2 class object
 * @param dest Pointer to the destination of data
 * @param size Size of data, see description
 * @return 1 if success; 0 is fail (buffer empty)
 */
int fifo2_read(Fifo2 this, void* dest, size_t size);

/** Get the read side pointer to manually read from the Fifo2. 
 * This function will block until there is valid internal buffer. 
 * Call fifo2_read_finish() to finish the reading so the write side can reuse it. 
 * @param this This Fifo2 class object
 * @return Read side pointer
 */
void* fifo2_read_start(Fifo2 this);

/** Finish the manual read from the Fifo2. 
 * Use with fifo2_read_start(). 
 * @param this This Fifo2 class object
 */
void fifo2_read_finish(Fifo2 this);

/** Destroy the Fiso2 class object, free all resource
 * @param this This Fifo2 class object
 */
void fifo2_destroy(Fifo2 this);

#endif /* #ifndef INCLUDE_COMMON_H */