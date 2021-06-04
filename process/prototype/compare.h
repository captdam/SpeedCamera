/** Class - Compare.class. 
 * Compare new and old frame to detect moving. 
 */

#ifndef INCLUDE_COMPARE_H
#define INCLUDE_COMPARE_H

#include "common.h"

/** Compare class object data structure
 */
typedef struct Compare_ClassDataStructure* Compare;

/** Init a compare object, create buffer. 
 * @param size Size of buffer, same as size of the frame, in uint of pixel by pixel
 * @param roadPoint Pointer to the road points list (contains absolute address to neighbor points)
 * @return $this(Opaque) compare class object if success, null if fail
 */
Compare compare_init(const size2d_t size, road_t* roadPoint);

/** Compare the frame in buffer and src, ave speed map in dest. 
 * @param this This source class object
 * @param dest Where to save the speed map
 * @param src Where is the frame data
 */
void compare_process(Compare this, uint8_t* dest, uint8_t* src);

/** Destroy this compare class object, frees resources 
 * @param this This compare class object
 */
void compare_destroy(Compare this);


#endif /* #ifndef INCLUDE_COMPARE_H */