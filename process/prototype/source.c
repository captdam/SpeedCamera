#include <stdlib.h>
#include <stdio.h>

#include "source.h"

typedef struct Source_ClassDataStructure {
	FILE* fp;
	void* buffer;
	size_t width, height;
} Source;

Source* source_init(const char* imageFile, size_t width, size_t height) {
	Source* this = malloc(sizeof(Source));
	if (!this)
		return NULL;
	
	this->fp = fopen(imageFile, "rb");
	if (!(this->fp))
		return NULL;
	
	this->buffer = malloc(SOURCE_PIXELSIZE * width * height);
	if (!(this->buffer))
		return NULL;

	this->width = width;
	this->height = height;
	return this;
}

size_t source_read(Source* this) {
	return fread(this->buffer, SOURCE_PIXELSIZE, this->width * this->height, this->fp);
}

void* source_getRawBitmap(Source* this) {
	return this->buffer;
}

void source_destroy(Source* this) {
	fclose(this->fp);
	free(this->buffer);
	free(this);
}