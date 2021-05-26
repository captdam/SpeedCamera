#include <stdlib.h>
#include <stdio.h>

#include "source.h"

struct Source_ClassDataStructure {
	FILE* fp;
	uint8_t* buffer;
	vh_t info;
};

Source source_init(const char* videoFile, vh_t* info) {
	Source this = malloc(sizeof(struct Source_ClassDataStructure));
	if (!this)
		return NULL;

	this->fp = NULL;
	this->buffer = NULL;
	
	this->fp = fopen(videoFile, "rb");
	if (!this->fp) {
		source_destroy(this);
		return NULL;
	}

	if (!fread(&this->info, 1, sizeof(this->info), this->fp)) {
		source_destroy(this);
		return NULL;
	}

	this->buffer = malloc(this->info.width * this->info.colorScheme);
	if (!this->buffer) {
		source_destroy(this);
		return NULL;
	}

	*info = this->info;
	return this;
}

size_t source_read(Source this, uint8_t* dest) {
	if (this->info.colorScheme == 1) {
		return fread(dest, 1, this->info.height * this->info.width, this->fp);
	}

	size_t c = 0;
	for (size_t i = this->info.height; i; i--) {
		c += fread(this->buffer, this->info.colorScheme, this->info.width, this->fp);
		uint8_t* p = this->buffer;
		for (size_t j = this->info.width; j; j--) {
			uint8_t l = ( *(p++) + *(p++) + *(p++) ) / 3;
			*(dest++) = l;
		}
	}
	return c;
}

void source_destroy(Source this) {
	if (!this)
		return;
	
	if (this->fp) fclose(this->fp); //fclose(NULL) is undefined
	free(this->buffer);
	free(this);
}