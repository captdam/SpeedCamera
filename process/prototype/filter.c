#include <string.h>

#include "filter.h"

#define N_FRAC 4
const int8_t filter_kernel3_mask[][9] = { //Fixed point number
	{ /* 0: sharpen */ 
		+0 << N_FRAC | 0,	-1 << N_FRAC | 0,	+0 << N_FRAC | 0,
		-1 << N_FRAC | 0,	+5 << N_FRAC | 0,	-1 << N_FRAC | 0,
		+0 << N_FRAC | 0,	-1 << N_FRAC | 0,	+0 << N_FRAC | 0
	}, { /* 1: blur */ 
		+0 << N_FRAC | 1,	+0 << N_FRAC | 1,	+0 << N_FRAC | 1,
		+0 << N_FRAC | 1,	+0 << N_FRAC | 1,	+0 << N_FRAC | 1,
		+0 << N_FRAC | 1,	+0 << N_FRAC | 1,	+0 << N_FRAC | 1
	}, { /* 2: gaussian */ 
		+0 << N_FRAC | 1,	+0 << N_FRAC | 2,	+0 << N_FRAC | 1,
		+0 << N_FRAC | 2,	+0 << N_FRAC | 4,	+0 << N_FRAC | 2,
		+0 << N_FRAC | 1,	+0 << N_FRAC | 2,	+0 << N_FRAC | 1
	}, { /* 3: edge4 */ 
		+0 << N_FRAC | 0,	-1 << N_FRAC | 0,	+0 << N_FRAC | 0,
		-1 << N_FRAC | 0,	+4 << N_FRAC | 0,	-1 << N_FRAC | 0,
		+0 << N_FRAC | 0,	-1 << N_FRAC | 0,	+0 << N_FRAC | 0
	}
};
void filter_kernel3(uint8_t* dest, uint8_t* src, size2d_t size, enum filter_kernel3_mode filter_kernel3_mode) {
	const int8_t* filter = filter_kernel3_mask[filter_kernel3_mode];

	memset(dest, 0, size.width * sizeof(*dest));
	
	dest += size.width;
	uint8_t* oprand[] = {
		src - 1, src, src + 1,
		src + size.width - 1, src + size.width, src + size.width + 1, 
		src + 2 * size.width - 1, src + 2 * size.width, src + 2 * size.width + 1
	};

	for (size_t i = size.height - 2; i; i--) {
		*(dest++) = 0;
		for (int k = 9; k;)
			oprand[--k]++;

		for (size_t j = size.width - 2; j; j--) {
			int16_t l = 0;
			for (int k = 9; k;) {
				l += *oprand[--k] * filter[k];
				oprand[k]++;
			}
			*(dest++) = l > 0 ? l >> N_FRAC : 0;
		}

		*(dest++) = 0;
		for (int k = 9; k;)
			oprand[--k]++;
	}

	memset(dest, 0, size.width * sizeof(*dest));
}