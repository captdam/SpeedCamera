#include "denoise.h"

void denoise_lowpass3(luma_t* image, size2d_t size) {
	for (size_t y = 1; y < size.height - 1; y++) {
		for (size_t x = 1; x < size.width - 1; x++) {
			luma_t up = image[ (y-1) * size.width + x ];
			luma_t down = image[ (y+1) * size.width + x ];
			luma_t left = image[ y * size.width + x - 1 ];
			luma_t right = image[ y * size.width + x + 1 ];
			luma_t center = image[ y * size.width + x ];
			if ( (up + down + left + right) / 4 < center ) {
				image[ y * size.width + x ] = (up + down + left + right) / 4;
			}
		}
	}
}