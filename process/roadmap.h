/** Class - Roadmap.class. 
 * Road information: road points in screen-domain and road-domain. 
 * Road mesh also indicates focus region. 
 * Project mesh used to transform image between prospective and orthographicview. 
 */

#ifndef INCLUDE_ROADMAP_H
#define INCLUDE_ROADMAP_H

#include "common.h"

/** Roadmap class object data structure
 */
typedef struct Roadmap_ClassDataStructure* Roadmap;

/** Header from roadmap file
 */
typedef struct Roadmap_Header {
	int16_t width, height; //Frame size in pixel, also used to calculate the size of this file. ASCII meta data append can be ignored
	float searchThreshold; //Max distance an object can travel between two frame, for search distance
	float orthoPixelWidth; //Pixel width is same for all pixels in orthographic view
	unsigned int searchDistanceXOrtho; //Max distance in x-axis in pixel an object can move under the speed threshold, in orthographic view, same for all pixels
} roadmap_header;

/** Data format from roadmap (table1): Road-domain geographic data in perspective view and orthographic view
 */
typedef struct Roadmap_Table1 {
	float px, py;
	float ox, oy;
} roadmap_t1;

/** Data format from roadmap (table2)
 */
typedef struct Roadmap_Table2 {
	unsigned int searchDistanceXPersp; //Max distance in x-axis in pixel an object can move under the speed threshold, in perspective view
	unsigned int searchDistanceY; //Max distance in y-axis in pixel an object can move under the speed threshold, same in both view
	unsigned int lookupXp2o, lookupXo2p; //Projection lookup table. Y-crood in orthographic and perspective views are same
} roadmap_t2;

/** Data format from roadmap (road points list)
*/
typedef struct Roadmap_Point_t {
	float roadX, roadY; //Road-domain coord
	unsigned int screenX, screenY; //Screen-domain coord
} roadmap_point_t;

/** Init a roadmap object, allocate memory for data read from map file. 
 * @param roadmapFile Directory to a binary coded file contains road-domain data
 * @param size Number of pixels in the video frame, used to check the program cmd againest the roadmap file
 * @return $this(Opaque) roadmap class object upon success. If fail, free all resource and return NULL
 */
Roadmap roadmap_init(const char* roadmapFile, size2d_t size);

/** Get a pointer to the vertices array of focus region. 
 * A vertex contains 2 float value: x-pos on screen, y-pos on screen. Top-left corner is {0.0f, 0.0f}; Bottom-right corner is {1.0f, 1.0f}. 
 * A number of triangles are used to connect vertices into mesh, each triangle requires 3 vertices. The order of the vertices is triangle fan. 
 * The program should only process objects on the road, which is in the mesh (focus region). 
 * @param this This roadmap class object
 * @param size Pass by reference: Number of vertex (1 vertex = 2 float)
 * @return Pointer to vertices array
 */
float* roadmap_getFocusRegion(Roadmap this, size_t* size);

/** Get the header of the roadmap file. 
 * @param this This roadmap class object
 * @return Roadmap file header
 */
roadmap_header roadmap_getHeader(Roadmap this);

/** Get the roadmap table 1, a pointer to a 2-D array of vec4, same size as the video frame. 
 * Data format from roadmap (table1): Road-domain geographic data in perspective view and orthographic view. 
 * @param this This roadmap class object
 * @return Pointer to the data
 */
roadmap_t1* roadmap_getT1(Roadmap this);

/** Get the roadmap table 2, a pointer to a 2-D array of ivec4, same size as the video frame. 
 * Data format from roadmap (table2): See struct dedine. 
 * @param this This roadmap class object
 * @return Pointer to the data
 */
roadmap_t2* roadmap_getT2(Roadmap this);

/** Destroy this roadmap class object, frees resources. 
 * @param this This roadmap class object
 */
void roadmap_destroy(Roadmap this);


#endif /* #ifndef INCLUDE_ROADMAP_H */