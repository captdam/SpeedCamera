/** Class - project.class. 
 * Camera may be shift in windy weather. 
 * This project image from camera-domain (clip-space, shifted by wind) to viewer-domain (stable). 
 */

#ifndef INCLUDE_PROJECT_H
#define INCLUDE_PROJECT_H

#include <inttypes.h>

#include "common.h"

/** Project class object data structure
 */
typedef struct Project_ClassDataStructure* Project;

/** Init a project object. 
 * This filter is used to project input image in camera-domain (buffer of edge.class), which may be shifted due to wind; 
 * to a view-domin, which is stable to the groud. 
 * This object will store the projected image in buffer, horizontal and vertical FOV of this buffer is fovX + 2 * marginX. 
 * @param edge Pointer to edge object's buffer
 * @param cameraSize Width and height of the camera image (not the clipped edge image)
 * @param fovH Horizontal field of view of the camera, in degree
 * @param fovV Vertical field of view, in degree
 * @param marginH Horizontal FOV margin, in degree
 * @param marginV Vertical FOV margin, in degree
 * @return $this(Opaque) project class object if success, null if fail
 */
Project project_init(luma_t* edge, size2d_t cameraSize, float fovH, float fovV, float marginH, float marginV);

/** Project edge image in camera-domain from edge object's buffer, to
 * viewer-domain and save it in this object's buffer, based on camera's position. 
 * Use project_getProjectImage() to get pointer of the buffer. 
 * @param this This edge class object
 * @param yaw Yaw of the camera
 * @param pitch Pitch of the camera
 */
void project_process(Project this, float yaw, float pitch);

/** Get the size of the buffer. 
 * Here is size of the projected image.
 * @param this This project class object
 * @return Size of the buffer in pixel
 */
size2d_t project_getProjectSize(Project this);

/** Get the pointer to buffer. 
 * Here is where the projected image saved.
 * @param this This project class object
 * @return A pointer to the filtered image saved in this object's buffer
 */
luma_t* project_getProjectImage(Project this);

/** Destroy this project class object. 
 * @param this This project class object
 */
void project_destroy(Project this);

#endif /* #ifndef INCLUDE_PROJECT_H */