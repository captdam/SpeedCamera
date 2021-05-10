/** Class - project.class. 
 * Camera may be shift in windy weather. 
 * This projects image from camera-domain (clip-space, shifted by wind) to viewer-domain (stable). 
 */

#ifndef INCLUDE_PROJECT_H
#define INCLUDE_PROJECT_H

#include <inttypes.h>

#include "common.h"

/** Project class object data structure
 */
typedef struct Project_ClassDataStructure* Project;

/** Init a project object. 
 * This filter is used to project camera-domain image from the camera, which may be shifted due to wind; 
 * to viewer-domin, which is stable to the ground. 
 * This object will store the projected image in buffer, horizontal and vertical FOV of this buffer is fovX + 2 * marginX. 
 * @param input Pointer to previous stage's buffer
 * @param inputSize Width and height of the input image
 * @param fovH Horizontal field of view of the camera, in degree
 * @param fovV Vertical field of view, in degree
 * @param marginH Horizontal FOV margin, in degree
 * @param marginV Vertical FOV margin, in degree
 * @return $this(Opaque) project class object if success, null if fail
 */
Project project_init(luma_t* input, size2d_t inputSize, float fovH, float fovV, float marginH, float marginV);

/** Project edge image in camera-domain from previous stage's buffer, to 
 * viewer-domain and save it in this object's buffer, based on camera's statue. 
 * Use project_getProjectImage() to get pointer of the buffer. 
 * @param this This edge class object
 * @param yaw Yaw of the camera
 * @param pitch Pitch of the camera
 * @param roll Roll of the camera
 */
void project_process(Project this, float yaw, float pitch, float roll);

/** Get the size of the buffer, in uint of pixel-by-pixel. 
 * Here is size of the projected image, it is based on the input size, input FOV and margin FOV. Thiis value will not change. 
 * @param this This project class object
 * @return Size of the buffer in pixel
 */
size2d_t project_getProjectSize(Project this);

/** Get the pointer to buffer. 
 * Here is where the projected image saved. This address will not change. 
 * @param this This project class object
 * @return A pointer to the filtered image saved in this object's buffer
 */
luma_t* project_getProjectImage(Project this);

/** Destroy this project class object. 
 * @param this This project class object
 */
void project_destroy(Project this);

#endif /* #ifndef INCLUDE_PROJECT_H */