/** Class - Speedometer.class. 
 * A class used to display speed on screen. 
 * This class is used to generate human readable output. 
 * This class use glyph to represent the speed, there are (and must be) 256 possible glyphs. 
 * There is no restruction on the size of the glyph, but all of them should have same size. 
 * The program uses RGBA8 format when reading the file; hence the glyphs can be colored. 
 */

#ifndef INCLUDE_SPEEDOMETER_H
#define INCLUDE_SPEEDOMETER_H

#include "common.h"

/** Speedometer class object data structure
 */
typedef struct Speedometer_ClassDataStructure* Speedometer;

/** Init a speedometer object, allocate memory for data read from bitmap file. 
 * @param bitmap Directory to a bitmap file, must be in plain RGBA8 raw data format
 * @param size Size of the bitmap file: depth = number of bitmap (must be 256); width and height = size of each glyph, in unit of pixel
 * @param count Number of speedometer in horizontal and vertical direction
 * @return $this(Opaque) speedometer class object upon success. If fail, free all resource and return NULL
 */
Speedometer speedometer_init(const char* bitmap, size3d_t size, size2d_t count);

/** Get a pointer to the bitmap glyph loaded from file, which can be upload to GPU as texture
 * @param this This speedometer class object
 * @return Pointer to the bitmap glyph
 */
void* speedometer_getBitmap(Speedometer this);

/** Get a pointer to the vertices array of the speedometer. 
 * A vertex contains 2 float value: x-pos of an individual speedometer , y-pos of an individual speedometer. 
 * Top-left corner is {-0.5f/count.x, -0.5f/count.y}; Bottom-right corner is {+0.5f/count.x, +0.5f/count.y}. 
 * The value is designed for GL, using the Interpolation feature when passing value from vertex shader to fragment shader. 
 * 2 triangles are used to connect vertices into rectangular mesh, each triangle requires 3 vertices. 
 * @param this This speedometer class object
 * @param size Pass by reference: Number of vertex (1 vertex = 2 float), must pass 4 back
 * @return Pointer to vertex array
 */
float* speedometer_getVertices(Speedometer this, size_t* size);

/** Get a pointer to the indices array of the speedometer. 
 * 2 triangles are used to connect vertices into rectangular mesh, each triangle requires 3 vertices. 
 * An index contains 3 unsigned ints, representing the order of vertices in the vertices array. 
 * @param this This speedometer class object
 * @param size Pass by reference: Number of index (1 index = 3 uint), must be pass 2 back
 * @return Pointer to indices array
 */
unsigned int* speedometer_getIndices(Speedometer this, size_t* size);

/** Get the pointer to the offset (instance) array of the speedometer.
 * Multiple speedometers are used, this array defines the position of them. 
 * @param this This speedometer class object
 * @param size Pass by reference: Number of speedometers (instances) (1 offset = 2 float)
 * @return Pointer to the offsets array
 */
float* speedometer_getOffsets(Speedometer this, size_t* size);

/** Destroy this speedometer class object, frees resources. 
 * @param this This speedometer class object
 */
void speedometer_destroy(Speedometer this);


#endif /* #ifndef INCLUDE_SPEEDOMETER_H */