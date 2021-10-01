/** Class - Speedometer.class. 
 * A class used to display speed on screen. 
 * This class is used to generate human readable output. 
 * The program uses RGBA8 format when reading the file; hence the glyphs can be colored and can have aplha channel. 
 */

#ifndef INCLUDE_SPEEDOMETER_H
#define INCLUDE_SPEEDOMETER_H

#include "common.h"

/** Speedometer class object data structure
 */
typedef struct Speedometer_ClassDataStructure* Speedometer;

/** Init a speedometer object, allocate memory, read speedometer glyph from bitmap file, init mesh. 
 * The bitmap must contains 256 glyphs, 16 rows in total, each row contains 16 glyphs. 
 * There is no restruction on the size of the glyph, but all of them should have same size. With one exception: 16 * width and 16 * height should not exceed the GPU's texture size limit. 
 * @param bitmapfile Directory to a bitmap file, must be in plain RGBA8 raw data format
 * @param size Size of the bitmap file (must be RGBA): in unit of pixel
 * @param count Number of speedometer in horizontal and vertical direction
 * @return $this(Opaque) speedometer class object upon success. If fail, free all resource and return NULL
 */
Speedometer speedometer_init(const char* bitmapfile, size2d_t size, size2d_t count);

/** Get a pointer to the bitmap glyph loaded from file, which can be upload to GPU as texture. 
 * This 2D bitmap contains a collection of 256 glpphs. 16 glyphs in one row, and there are 16 rows. 
 * The order is from small to large: left to right, then top to bottom. 
 * @param this This speedometer class object
 * @return Pointer to the bitmap glyph (format = RGBA8, uint32_t)
 */
void* speedometer_getBitmap(Speedometer this);

/** Get a pointer to the vertices array of the speedometer. 
 * A vertex contains 4 float value: x-pos and y-pos of an individual speedometer on screen, x-pos and y-pos of an individual speedometer's texture. 
 * 2 triangles are used to connect 4 vertices into 1 rectangular mesh, each triangle requires 3 vertices. 
 * @param this This speedometer class object
 * @param size Pass by reference: Number of vertex (1 vertex = 4 float), should pass (4 * count) back
 * @return Pointer to vertex array
 */
float* speedometer_getVertices(Speedometer this, size_t* size);

/** Get a pointer to the indices array of the speedometer. 
 * 2 triangles are used to connect 4 vertices into 1 rectangular mesh, each triangle requires 3 vertices. 
 * An index contains 3 unsigned ints, representing the index of 3 vertices in the vertices array which form a triangle. 
 * @param this This speedometer class object
 * @param size Pass by reference: Number of index (1 index = 3 uint), should be pass (2 * count) back
 * @return Pointer to indices array
 */
unsigned int* speedometer_getIndices(Speedometer this, size_t* size);

/** Destroy this speedometer class object, frees resources. 
 * @param this This speedometer class object
 */
void speedometer_destroy(Speedometer this);


#endif /* #ifndef INCLUDE_SPEEDOMETER_H */