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
typedef struct Roadmap {
	struct Roadmap_Header{
		uint32_t width, height; //Frame size in pixel
		uint32_t pCnt; //Number of road points
		uint32_t fileSize; //Size of roadmap file without meta data, in byte
	} header;
	struct RoadPoint { //[1...-5]Region of interest vertices {screen-x, screen-y} in prespective view [-4..-1] in orthographic view
		float sx;
		float sy;
	}* roadPoints;
	struct Roadmap_Table1 { //Road-domain geographic data in perspective view and orthographic view
		float px, py; //Road-domain geographic data in perspective view
		float ox; //Road-domain geographic data in orthographic view, y-coord of orthographic view is same as y-coord of perspective view
		float pw; //Perspective pixel width (inverse): distance * this = number of pixels for that distance
	}* t1;
	struct Roadmap_Table2 { //Search limit, perspective and orthographic projection map
		float searchLimitUp, searchLimitDown; //Up/down search limit y-coord for displacement measure, Single frame limit on left pixel, multi frame limit on right pixel, based on param of roadmap_post()
		float lookupXp2o, lookupXo2p; //Projection lookup table. Y-crood in orthographic and perspective views are same
	}* t2;
} roadmap;

#define ROADMAP_DEFAULTSTRUCT {.roadPoints = NULL}

/** Init a roadmap object, allocate memory for data read map file. 
 * @param roadmapFile Directory to a binary coded file contains road-domain data
 * @param statue Return-by-reference error message if this function fail, NULL if success
 * @return roadmap class object upon success
 */
roadmap roadmap_init(const char* const roadmapFile, char** const statue);

/** Post processing the table data. 
 * The table provides scene dependent data, but some data is config depedent. 
 * Some data can be further optimized for the program. 
 * Generate the search limit in roadmap table 2, used for object displacement measurement. 
 * @param this This roadmap class object
 * @param limitSingle Object max road-domain displacement (meter) in one frame
 * @param limitMulti Object max road-domain displacement (meter) in multiple frame
 */
void roadmap_post(roadmap* this, float limitSingle, float limitMulti);

/** Destroy this roadmap class object, frees resources (all tables). 
 * @param this This roadmap class object
 */
void roadmap_destroy(roadmap* this);


#endif /* #ifndef INCLUDE_ROADMAP_H */