/** Class - Speedometer.class. 
 * A class used to process and display speed on screen. 
 * This class is used to sample the speed data and generate human readable output. 
 */

#ifndef INCLUDE_SPEEDOMETER_H
#define INCLUDE_SPEEDOMETER_H

#include "common.h"

/** Speedometer class object data structure
 */
typedef struct Speedometer_ClassDataStructure* Speedometer;

/** Init a speedometer object, allocate memory, read speedometer glyph from bitmap file, init mesh. 
 * The glyph bitmap file must contains 256 glyphs, 16 rows in total, each row contains 16 glyphs. This bitmap must be in RGBA8 (uint32_t) format. 
 * There is no restriction on the size of the glyph, but all of them should have same size. 
 * The whole frame is divided into a number of rectangular speedometer, count.x in horizontal direction, count.y in vertical direction. 
 * @param glyphs Directory to a glyphs file, must be in plain RGBA8 raw data format
 * @param count Number of speedometer in horizontal and vertical direction
 * @return $this(Opaque) speedometer class object upon success. If fail, free all resource and return NULL
 */
Speedometer speedometer_init(const char* glyphs, size2d_t count);

/** Get a pointer to the glyph loaded from file, which can be upload to GPU as texture. 
 * This 2D bitmap contains a collection of 256 glpphs. 16 glyphs in one row, and there are 16 rows. 
 * The order is from small to large: left to right, then top to bottom. 
 * @param this This speedometer class object
 * @param size Size of each glyph, pass-by-reference
 * @return Pointer to the bitmap glyph (format = RGBA8, uint32_t)
 */
uint32_t* speedometer_getGlyph(Speedometer this, size2d_t* size);

/** Get a pointer to the vertices array of the speedometer. 
 * A vertex contains 4 float value: left-x, top-y, right-x and bottom-y coordinates of each rectangular speedometer. 
 * Which is: {{left-x, top-y, right-x, bottom-y}, ...}. 
 * Should use gl_points. 
 * @param this This speedometer class object
 * @param size Pass by reference: Number of vertex (1 vertex = 4 float), should pass (count.x * count.y) back
 * @return Pointer to vertex array
 */
float* speedometer_getVertices(Speedometer this, size_t* size);

/** Get a pointer to the indices array of the speedometer. 
 * Each speedometer only uses 1 vertex; hence the indices is just an array of number in row. 
 * An index contains 1 unsigned int, representing the index of 1 vertice in the vertices array which form a point. 
 * @param this This speedometer class object
 * @param size Pass by reference: Number of index (1 index = 1 uint), should pass (count.x * count.y) back
 * @return Pointer to indices array
 */
unsigned int* speedometer_getIndices(Speedometer this, size_t* size);

/** Destroy this speedometer class object, frees resources. 
 * @param this This speedometer class object
 */
void speedometer_destroy(Speedometer this);


#endif /* #ifndef INCLUDE_SPEEDOMETER_H */