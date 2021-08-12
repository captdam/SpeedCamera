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
 * @param mapfile Name of roadmap file
 * @return $this(Opaque) roadmap class object upon success. If fail, free all resource and return NULL
 */
Roadmap roadmap_init(const char* mapfile);

/** Get a pointer to the vertices array. 
 * A vertex has contains 4 float value: x-pos on screen, y-pos on screen, x-pos on road, y-pos on road. 
 * Top-left corner is {0.0f, 0.0f}; Bottom-right corner is {1.0f, 1.0f}. 
 * The value is designed for GL, using the Interpolation feature when passing value from vertex shader to fragment shader. 
 * Number of triangles are used to connect vertices into mesh, each triangle requires 3 vertices. 
 * The program should only process data in the mesh (focus region). 
 * @param this This roadmap class object
 * @param size Pass by reference: Number of vertex (1 vertex = 4 float)
 * @return Pointer to vertices array
 */
float* roadmap_getVertices(Roadmap this, size_t* size);

/** Get a pointer to the indices array. 
 * Number of triangles are used to connect vertices into mesh, each triangle requires 3 vertices. 
 * A index contains 3 unsigned ints, representing the order of vertices in the vertices array. 
 * The program should only process data in the mesh (focus region). 
 * @param this This roadmap class object
 * @param size Pass by reference: Number of index (1 index = 3 uint)
 * @return Pointer to indices array
 */
unsigned int* roadmap_getIndices(Roadmap this, size_t* size);

/** Destroy this roadmap class object, frees resources. 
 * @param this This roadmap class object
 */
void roadmap_destroy(Roadmap this);


#endif /* #ifndef INCLUDE_ROADMAP_H */