/** Class - Compare.class. 
 * Compare new and old frame to detect moving. 
 */

#ifndef INCLUDE_COMPARE_H
#define INCLUDE_COMPARE_H

#include "common.h"

/** Compare class object data structure
 */
typedef struct Compare_ClassDataStructure* Compare;

/** Init a compare object, create buffer and analysis location map. 
 * @param size Size of buffer, uint of pixel by pixel
 * @param mapfile Name of file that indicates whcih pixel to look
 * @return $this(Opaque) compare class object if success, null if fail
 */
Compare compare_init(const size2d_t size, const char* mapfile);

/** Read a frame from the source file, save the gray scale image to the dest. 
 * @param this This source class object
 * @return Number of bytes fetched from source file
 */
void compare_process(Compare this, uint8_t* dest, uint8_t* src);

/** Destroy this compare class object, frees resources 
 * @param this This compare class object
 */
void compare_destroy(Compare this);


#endif /* #ifndef INCLUDE_COMPARE_H */