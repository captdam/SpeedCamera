/** Class - compare.class. 
 * By comparing the displacement of edge, we can know the speed of the object in viewer-domain. 
 * Then apply triagmitry, we can find the object's speed in world-domain. 
 */

#ifndef INCLUDE_COMPARE_H
#define INCLUDE_COMPARE_H

#include <inttypes.h>

#include "common.h"

/** Project class object data structure
 */
typedef struct Compare_ClassDataStructure* Compare;

/** Init a compare object. 
 * @param input Pointer to previous stage's buffer
 * @param size Width and height of the input image
 * @param maxSpeed Highest possible speed, in km/h
 * @param fps FPS of the video
 * @return $this(Opaque) compare class object if success, null if fail
 */
Compare compare_init(luma_t* input, size2d_t size, float maxSpeed, float fps);

/** Setup location map. 
 * Use this function to pass world information to this object. 
 */
void compare_setLocationMap(Compare this, loc3d_t* location);

/** Fetch frame from previous stage (new frame), then compare new and old frame. 
 * @param this This edge class object
 */
void compare_process(Compare this);

/** Get size of the buffer. This value will not change. 
 */
size2d_t compare_getMapSize(Compare this);

/** Get the pointer to speed map. 
 * Here is where the speed map saved. This address will not change. 
 * This map marks the speed of object in world-domain. 
 * @param this This compare class object
 * @return A pointer to the filtered image saved in this object's buffer
 */
uint8_t* compare_getSpeedMap(Compare this);

/** Destroy this compare class object. 
 * @param this This compare class object
 */
void compare_destroy(Compare this);

#endif /* #ifndef INCLUDE_COMPARE_H */