#include <time.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

uint64_t nanotime() {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000000000LLU + t.tv_nsec;
}

int inBox(const int x, const int y, const int left, const int right, const int top, const int bottom, const int strict) {
	if (strict == 0)
		return x >= left && x <= right && y >= top && y <= bottom;
	if (strict > 0)
		return x > left && x < right && y > top && y < bottom;
	return x >= left && x < right && y >= top && y < bottom;
}