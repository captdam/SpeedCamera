/** Class - Filter.class. 
 * Numbers of 2D filter used in image processing. 
 */

#ifndef INCLUDE_FILTER_H
#define INCLUDE_FILTER_H

#include "common.h"

enum filter_kernel3_mode {
	filter_kernel3_sharpen = 0,
	filter_kernel3_blur = 1,
	filter_kernel3_gaussian = 2,
	filter_kernel3_edge4 = 3
};

void filter_kernel3(uint8_t* dest, uint8_t* source, size2d_t size, enum filter_kernel3_mode);


#endif /* #ifndef INCLUDE_FILTER_H */