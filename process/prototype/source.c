#include <stdlib.h>
#include <stdio.h>

#include "source.h"

struct Source_ClassDataStructure {
	FILE* fp;
	void* buffer;
	size_t frameSize;
};

Source source_init(const char* imageFile, size2d_t resolution, size_t bytePerPixel) {
	Source this = malloc(sizeof(struct Source_ClassDataStructure));
	if (!this)
		return NULL;
	
	this->fp = NULL;
	this->buffer = NULL;
	this->frameSize = bytePerPixel * resolution.width * resolution.height;

	this->fp = fopen(imageFile, "rb");
	if (!(this->fp)) {
		source_destroy(this);
		return NULL;
	}

	this->buffer = malloc(this->frameSize);
	if (!(this->buffer)) {
		source_destroy(this);
		return NULL;
	}

	return this;
}

size_t source_read(Source this) {
	return fread(this->buffer, 1, this->frameSize, this->fp);
}

void* source_getRawBitmap(Source this) {
	return this->buffer;
}

void source_destroy(Source this) {
	if (!this)
		return;
	
	if (this->fp) fclose(this->fp); //fclose(NULL) is undefined
	free(this->buffer); //free(NULL) is safe
	free(this);
}