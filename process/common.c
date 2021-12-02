#include <time.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

uint64_t nanotime() {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000000000LLU + t.tv_nsec;
}

int inBox(size2d_t ptr, size2d_t leftTop, size2d_t rightBottom, int strict) {
	size_t x = ptr.x, y = ptr.y;
	size_t left = leftTop.x, right = rightBottom.x;
	size_t top = leftTop.y, bottom = rightBottom.y;
	if (strict == 0)
		return x >= left && x <= right && y >= top && y <= bottom;
	else if (strict > 0)
		return x > left && x < right && y > top && y < bottom;
	else
		return x >= left && x < right && y >= top && y < bottom;
}