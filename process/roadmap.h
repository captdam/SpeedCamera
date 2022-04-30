/** Class - Roadmap.class. 
 * Road mesh also indicates focus region. 
 * Road information: road points associated to screen-domain pixel. 
 * Search limit for measure.
 * Project lookup used to transform image between prospective and orthographic view. 
 */

#ifndef INCLUDE_ROADMAP_H
#define INCLUDE_ROADMAP_H

#include <inttypes.h>

/** Roadmap class object data structure
 */
typedef struct Roadmap_ClassDataStructure* const Roadmap;

/** Header from roadmap file
 */
typedef struct Roadmap_Header {
	uint32_t width, height; //Frame size in pixel
	uint32_t pCnt; //Number of road points
	uint32_t fileSize; //Size of roadmap file without meta data, in byte
} roadmap_header;

/** Data format from roadmap: Table1: Road-domain geographic data in perspective view and orthographic view
 */
typedef struct Roadmap_Table1 {
	float px, py; //Road-domain geographic data in perspective view
	float ox; //Road-domain geographic data in orthographic view, y-coord of orthographic view is same as y-coord of perspective view
	float pw; //Perspective pixel width
} roadmap_t1;

/** Data format from roadmap: Table2: Search limit, perspective and orthographic projection map
 */
typedef struct Roadmap_Table2 {
	float searchLimitUp, searchLimitDown; //Up/down search limit y-coord
	float lookupXp2o, lookupXo2p; //Projection lookup table. Y-crood in orthographic and perspective views are same
} roadmap_t2;

/** Data format from roadmap: Focus region points
*/
typedef struct Roadmap_Point_t {
	float rx, ry; //Road-domain location (m)
	float sx, sy; //Screen domain position (0.0 for left and top, 1.0 for right and bottom)
} roadmap_point_t;

/** Init a roadmap object, allocate memory for data read from map file. 
 * @param roadmapFile Directory to a binary coded file contains road-domain data
 * @param statue If not NULL, return error message in case this function fail
 * @return $this(Opaque) roadmap class object upon success. If fail, free all resource and return NULL
 */
Roadmap roadmap_init(const char* const roadmapFile, char** const statue);

/** Generate the search limit in roadmap table 2. 
 * @param this This roadmap class object
 * @param limit Limit in road-domain in meter
 */
void roadmap_genLimit(const Roadmap this, float limit);

/** Get the header of the roadmap file. 
 * Header contains roadmap size (width nad height) in px, and number of road points that enclose focus region. 
 * @param this This roadmap class object
 * @return Roadmap file header
 */
roadmap_header roadmap_getHeader(const Roadmap this);

/** Get the roadmap table 1, a 2-D array of float[4]. 
 * Data from roadmap (table1): Road-domain geographic data in perspective view and orthographic view. 
 * @param this This roadmap class object
 * @return Pointer to the data
 */
roadmap_t1* roadmap_getT1(const Roadmap this);

/** Get the roadmap table 2, a 2-D array of float[4]. 
 * Data from roadmap (table2): Search distance, prospective and orthographic projection map. 
 * @param this This roadmap class object
 * @return Pointer to the data
 */
roadmap_t2* roadmap_getT2(const Roadmap this);

/** Get the road points array (focus region). 
 * Road points defines the boundary of the road. 
 * For the purpose of this program, only data encircled by the road points (which means objects on the road) should be processed. 
 * Anything outside of the boundary should be ignored. 
 * Each road points contains 2 float (x and y coord on the screen, normalized to [0,1]). 
 * First header.pCnt - 4 road points are for perspective view, last 4 are for orthographic view. 
 * @param this This roadmap class object
 * @return Pointer to road points array
 */
float* roadmap_getRoadPoints(const Roadmap this);

/** Destroy this roadmap class object, frees resources (all tables). 
 * @param this This roadmap class object
 */
void roadmap_destroy(const Roadmap this);


#endif /* #ifndef INCLUDE_ROADMAP_H */