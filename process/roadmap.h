/** Class - Roadmap.class. 
 * Road information: road points in screen-domain and road-domain.
 * Road map also indicates focus region. 
 */

#ifndef INCLUDE_ROADMAP_H
#define INCLUDE_ROADMAP_H

#include "common.h"

/** Roadmap class object data structure
 */
typedef struct Roadmap_ClassDataStructure* Roadmap;

/** Init a roadmap object, allocate memory for data read from map file. 
 * @param focusRegionFile Directory to a human-readable file contains focus region (use indexed triangle mesh, normalized [0,1])
 * @param distanceMapFile Directory to a binary coded file contains road-domain geographic data (use 2D float array), size should match with the video frame
 * @param size Number of pixels in the video frame
 * @return $this(Opaque) roadmap class object upon success. If fail, free all resource and return NULL
 */
Roadmap roadmap_init(const char* focusRegionFile, const char* distanceMapFile, size_t size);

/** Get a pointer to the vertices array of focus region. 
 * A vertex contains 2 float value: x-pos on screen, y-pos on screen. 
 * Top-left corner is {0.0f, 0.0f}; Bottom-right corner is {1.0f, 1.0f}. 
 * The value is designed for GL, using the Interpolation feature when passing value from vertex shader to fragment shader. 
 * A number of triangles are used to connect vertices into mesh, each triangle requires 3 vertices. 
 * The program should only process data in the mesh (focus region). 
 * @param this This roadmap class object
 * @param size Pass by reference: Number of vertex (1 vertex = 2 float)
 * @return Pointer to vertices array
 */
float* roadmap_getVertices(Roadmap this, size_t* size);

/** Get a pointer to the indices array of focus region. 
 * A number of triangles are used to connect vertices into mesh, each triangle requires 3 vertices. 
 * An index contains 3 unsigned ints, representing the order of vertices in the vertices array. 
 * The program should only process data in the mesh (focus region). 
 * @param this This roadmap class object
 * @param size Pass by reference: Number of index (1 index = 3 uint)
 * @return Pointer to indices array
 */
unsigned int* roadmap_getIndices(Roadmap this, size_t* size);

/** Get the road-domain geographic data, a pointer to a 2-D array of road points, same size as the video frame. 
 * Each road points constains 4 float number data: position-x, position-y, searchRegion-x, searchRegion-y. 
 * Position-xy is the geographic position of that point on the road. 
 * SearchRegion-xy is the x- and y-axis distance in unit of pixel that the program should search when finding displacement. 
 * The distance from the current road point to the edge of search region is the threshold. 
 * @param this This roadmap class object
 * @return Pointer to the road-domain geographic data
 */
float* roadmap_getGeographic(Roadmap this);

/** Get the threshold of the roadmap. 
 * Threshold is the max distance an object cound travel in road-domain between wo frames. 
 * This value is defined when generating the roadmap file. 
 * @param this This roadmap class object
 * @return Threshold
 */
float roadmap_getThreshold(Roadmap this);

/** Destroy this roadmap class object, frees resources. 
 * @param this This roadmap class object
 */
void roadmap_destroy(Roadmap this);


#endif /* #ifndef INCLUDE_ROADMAP_H */