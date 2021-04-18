/** Class - compare.class. 
 * By comparing the speed of pixel changing, we can know the speed of the object in viewer-domain. 
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
 * This object has 3 buffers: 
 * One is used to store project frame from project object;
 * One is used to store project frame of last frame (for comparing);
 * One is used to mark the duration of the object stay in that pixel. 
 * @param project Pointer to project object's buffer
 * @param projectSize Width and height of the project buffer
 * @param distance Distance of that pixel, speed = distance/duration
 * @return $this(Opaque) compare class object if success, null if fail
 */
Compare compare_init(luma_t* project, size2d_t projectSize, float* distance);

/** Fetch frame from project object into this object's buffer, then compare new and old buffer. 
 * @param this This edge class object
 */
void compare_process(Compare this);

/** Get the pointer to speed map. 
 * This map marks the speed of object in world-domain.
 * @param this This compare class object
 * @return A pointer to the filtered image saved in this object's buffer
 */
uint16_t* compare_getSpeedMap(Compare this);

/** Destroy this compare class object. 
 * @param this This compare class object
 */
void compare_destroy(Compare this);

#endif /* #ifndef INCLUDE_COMPARE_H */