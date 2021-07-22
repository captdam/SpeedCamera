#include <time.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

uint64_t nanotime() {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000000000LLU + t.tv_nsec;
}

/* == Fifo2 == Double buffer ================================================================ */

struct Util_DoubleBufferFIFO {
	unsigned int readCount, writeCount;
	size_t size;
	void* bufferA, * bufferB;
};

Fifo2 fifo2_init(size_t size) {
	Fifo2 this = malloc(sizeof(struct Util_DoubleBufferFIFO));
	if (!this)
		return NULL;
	
	this->readCount = 0;
	this->writeCount = 0;
	this->size = size;
	this->bufferA = NULL;
	this->bufferB = NULL;

	this->bufferA = malloc(size);
	this->bufferB = malloc(size);
	if (!this->bufferA || !this->bufferB) {
		fifo2_destroy(this);
		return NULL;
	}

	return this;
}

void fifo2_status(Fifo2 this, size_t* size, unsigned int* space) {
	*size = this->size;
	*space = 2 - (this->writeCount - this->readCount);
}

int fifo2_write(Fifo2 this, void* src, size_t size) {
	if (this->readCount == this->writeCount - 2) //Full
		return 0;
	
	memcpy(
		(uint8_t)this->writeCount & (uint8_t)0x01 ? this->bufferA : this->bufferB,
		src,
		size ? size : this->size
	);
	this->writeCount++;
	return 1;
}

void* fifo2_write_start(Fifo2 this) {
	while (this->readCount == this->writeCount - 2);
	return (uint8_t)this->readCount & (uint8_t)0x01 ? this->bufferA : this->bufferB;
}

void fifo2_write_finish(Fifo2 this) {
	this->writeCount++;
}

int fifo2_read(Fifo2 this, void* dest, size_t size) {
	if (this->readCount == this->writeCount) //Empty
		return 0;
	
	memcpy(
		dest,
		(uint8_t)this->readCount & (uint8_t)0x01 ? this->bufferA : this->bufferB,
		size ? size : this->size
	);
	this->readCount++;
	return 1;
}

void* fifo2_read_start(Fifo2 this) {
	while (this->readCount == this->writeCount);
	return (uint8_t)this->readCount & (uint8_t)0x01 ? this->bufferA : this->bufferB;
}

void fifo2_read_finish(Fifo2 this) {
	this->readCount++;
}

void fifo2_destroy(Fifo2 this) {
	if (!this)
		return;

	free(this->bufferA);
	free(this->bufferB);
	free(this);
}