/** Class - Roadmap.class. 
 * Road information: road points and their neighbor points. 
 */

#ifndef INCLUDE_ROADMAP_H
#define INCLUDE_ROADMAP_H

#include "common.h"

/** Roadmap class object data structure
 */
typedef struct Roadmap_ClassDataStructure* Roadmap;

/** Init a roadmap object, allocate memory and analysis roadmap. 
 * This class object contains two lists: road points list of road_t, neighbor points of neighbor_t. 
 * Use roadmap_getRoadpoint(this) to get a pointer to the road points list, length of this list is same as the frame size (width * height). 
 * Road points list contains the start and end address of all road points' assocated neighbor points list. 
 * The start and end address will be NULL if this road point is out of focus region. (In another word, has no neighbor point.) 
 * These addresses are absolute addresses, use them to access the neighbor points list. 
 * CAUTION: Because the road points list contains absolute address to neighbor points list, DO NOT ADAPT THIS CODE TO OTHER LANGUAGE! 
 * @param mapfile Name of roadmap file
 * @param size Size of the video frame
 * @return $this(Opaque) roadmap class object if success, null if fail
 */
Roadmap roadmap_init(const char* mapFile, size2d_t size);

/** Get the road points list. 
 * Size of the road points list is same as the video frame. 
 * This address will not change after init. 
 * @param this This source class object
 * @return Pointer to road points list
 */
road_t* roadmap_getRoadpoint(Roadmap this);

/** Destroy this roadmap class object, frees resources 
 * @param this This roadmap class object
 */
void roadmap_destroy(Roadmap this);


#endif /* #ifndef INCLUDE_ROADMAP_H */