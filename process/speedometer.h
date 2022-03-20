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
 * The glyph bitmap file must contains 256 glyphs, 16 rows in total, each row contains 16 glyphs. 
 * Order of glyphs must be left-to-right, top-to-bottom. 
 * This bitmap must be in RGBA8 (uint32_t/pixel) format. 
 * There is no restriction on the size of the each glyph, but all of them should have same size. 
 * @param glyphs Directory to a glyphs file, must be in plain RGBA8 raw data format
 * @param statue If not NULL, return error message in case this function fail
 * @return $this(Opaque) speedometer class object upon success. If fail, free all resource and return NULL
 */
Speedometer speedometer_init(const char* glyphs, char** statue);

/** Convert the glyph to array. 
 * By default, the glyph map layout is 16 * 16; in another word, a flat texture contains 256 small texture. This works for 2D texture. 
 * By calling this method, the flat 2D glyph map will be converted into 2D array texture; which means, there will be 256 different 2D textures in the array. 
 * Each of the 256 glyph can be access by using index (z-axis). This works for 2D array texture. 
 * @param this This speedometer class object
 */
void speedometer_convert(Speedometer this);


/** Get a pointer to the glyph. 
 * @param this This speedometer class object
 * @param size Width and height of each glyph, pass-by-reference
 * @return Pointer to the bitmap glyph
 */
uint32_t* speedometer_getGlyph(Speedometer this, unsigned int size[static 2]);

/** Destroy this speedometer class object, frees resources. 
 * @param this This speedometer class object
 */
void speedometer_destroy(Speedometer this);


#endif /* #ifndef INCLUDE_SPEEDOMETER_H */