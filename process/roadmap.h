/** Class - Roadmap.class. 
 * Road information: road points in screen-domain and road-domain. 
 * Road mesh also indicates focus region. 
 * Project lookup used to transform image between prospective and orthographic view. 
 */

#ifndef INCLUDE_ROADMAP_H
#define INCLUDE_ROADMAP_H

#include <inttypes.h>

/** Roadmap class object data structure
 */
typedef struct Roadmap_ClassDataStructure* Roadmap;

/** Header from roadmap file
 */
typedef struct Roadmap_Header {
	uint32_t width, height; //Frame size in pixel, also used to calculate the size of this file
} roadmap_header;

/** Data format from roadmap: Table1: Road-domain geographic data in perspective view and orthographic view
 */
typedef struct Roadmap_Table1 {
	float px, py; //Road-domain geographic data in prospective view
	float ox, oy; //Road-domain geographic data in orthographic view
} roadmap_t1;

/** Data format from roadmap: Table2: Search distance, prospective and orthographic projection map
 */
typedef struct Roadmap_Table2 {
	uint32_t searchLimitUp, searchLimitDown; //Up/down search limit y-coord
	uint32_t lookupXp2o, lookupXo2p; //Projection lookup table. Y-crood in orthographic and perspective views are same
} roadmap_t2;

/** Data format from roadmap: Number of points pair, focus region points
*/
typedef uint32_t roadmap_pCnt_t;
typedef struct Roadmap_Point_t {
	float rx, ry; //Road-domain location (m)
	float sx, sy; //Screen domain position (0.0 for left and top, 1.0 for right and bottom)
} roadmap_point_t;

/** Init a roadmap object, allocate memory for data read from map file. 
 * @param roadmapFile Directory to a binary coded file contains road-domain data
 * @param statue A pointer to message, used to return statue if this function fail
 * @return $this(Opaque) roadmap class object upon success. If fail, free all resource and return NULL
 */
Roadmap roadmap_init(const char* roadmapFile, char** statue);

/** Get the header of the roadmap file. 
 * @param this This roadmap class object
 * @return Roadmap file header
 */
roadmap_header roadmap_getHeader(Roadmap this);

/** Get the roadmap table 1, a 2-D array of float[4]. 
 * Data from roadmap (table1): Road-domain geographic data in perspective view and orthographic view. 
 * @param this This roadmap class object
 * @return Pointer to the data
 */
roadmap_t1* roadmap_getT1(Roadmap this);

/** Get the roadmap table 2, a 2-D array of unsigned int[4]. 
 * Data from roadmap (table2): Search distance, prospective and orthographic projection map. 
 * @param this This roadmap class object
 * @return Pointer to the data
 */
roadmap_t2* roadmap_getT2(Roadmap this);

/** Get the road points array (focus region). 
 * Road points defines the boundary of the road. 
 * For the purpose of this program, only data encircled by the road points (which means objects on the road) should be processed. 
 * Anything outside of the boundary should be ignored. 
 * The road points are in rong-order, which can be used directly by openGL using triangle fan mode. 
 * Number of road points is always a multiply of 2. Each road points contains 2 float (x and y coord on the screen, normalized to [0,1])
 * @param this This roadmap class object
 * @param size Pass by reference: Number of road points (1 vertex = 2 float)
 * @return Pointer to road points array
 */
float* roadmap_getRoadPoints(Roadmap this, unsigned int* size);

/** Destroy this roadmap class object, frees resources (all tables). 
 * @param this This roadmap class object
 */
void roadmap_destroy(Roadmap this);


#endif /* #ifndef INCLUDE_ROADMAP_H */