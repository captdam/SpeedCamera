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

/** Init a speedometer object, allocate memory, read speedometer glyph from bitmap file. 
 * The glyph bitmap file must contains 256 glyphs, 16 rows in total, each row contains 16 glyphs. This bitmap must be in RGBA8 (uint32_t) format. 
 * There is no restriction on the size of the glyph, but all of them should have same size. 
 * @param glyphs Directory to a glyphs file, must be in plain RGBA8 raw data format
 * @return $this(Opaque) speedometer class object upon success. If fail, free all resource and return NULL
 */
Speedometer speedometer_init(const char* glyphs);

/** Convert the glyph to array. 
 * By default, the glyph map layout is 16 * 16; in another word, a flat texture contains 256 small texture. This works for 2D texture. 
 * By calling this method, the flat 2D glyph map will be converted into 2D array texture; which means, there will be 256 different 2D textures in the array. 
 * Each of the 256 glyph can be access by using index (z-axis). This works for 2D array texture. 
 * @param this This speedometer class object
 * @return On success, return 1; if fail (out of memory), return 0
 */
int speedometer_convert(Speedometer this);


/** Get a pointer to the glyph loaded from file, which can be upload to GPU as texture. 
 * This 2D bitmap contains a collection of 256 glpphs. 16 glyphs in one row, and there are 16 rows. 
 * The order is from small to large: left to right, then top to bottom. 
 * @param this This speedometer class object
 * @param size Size of each glyph, pass-by-reference
 * @return Pointer to the bitmap glyph (format = RGBA8, uint32_t)
 */
uint32_t* speedometer_getGlyph(Speedometer this, size2d_t* size);

/** Destroy this speedometer class object, frees resources. 
 * @param this This speedometer class object
 */
void speedometer_destroy(Speedometer this);


#endif /* #ifndef INCLUDE_SPEEDOMETER_H */