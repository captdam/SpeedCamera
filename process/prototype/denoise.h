/** Class - denoise.class. 
 * A Image (2D) noise deduction filter. 
 */

#ifndef INCLUDE_DENOISE_H
#define INCLUDE_DENOISE_H

#include <inttypes.h>

#include "common.h"

/** 3*3 Low pass filter. [STATIC] 
 * This filter is used to remove high-frequency component in the image; in another word, dot noise. 
 * This filter clamp the luma of pixels by the mean of their up/down/left/right neighbor. 
 * This function will overwrite the given image. 
 * @param image Pointer to a 2D gray-scale image
 * @param size Size of that image
 */
void denoise_lowpass34(luma_t* image, size2d_t size);

/** High-value pass filter [STATIC] 
 * This filter is used to remove low-value component in the image. 
 * This function will overwrite the given buffer. 
 * @param buffer Pointer to a 2D buffer
 * @param size Size of that buffer
 * @param min Cut-off value
 */
void denoise_highValuePass8(uint8_t* buffer, size2d_t size, uint8_t min);

#endif /* #ifndef INCLUDE_DENOISE_H */