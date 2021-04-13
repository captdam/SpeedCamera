/** Size of each pixel of the source image 
 * 1: monochrome, grayscale 
 * 2: RGB565, not implemented yet 
 * 3: RGB, 8-bit each channel 
 * 4: RGBA, 8-bit each channel 
 */
#define SOURCEFILEPIXELSZIE 3

/** Internal data type used to store luma of each pixel
 */
typedef int16_t luma_t;