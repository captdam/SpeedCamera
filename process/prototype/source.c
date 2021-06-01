#include <stdlib.h>
#include <stdio.h>

#include "source.h"

struct Source_ClassDataStructure {
	FILE* fp;
	uint8_t* buffer;
	vh_t info;
	uint32_t (*readFunction)(Source, uint8_t*);
};

uint32_t source_read1(Source this, uint8_t* dest);
uint32_t source_read3(Source this, uint8_t* dest);
uint32_t source_read4(Source this, uint8_t* dest);

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

	switch (this->info.colorScheme) {
		case 1:	this->readFunction = source_read1; break;
		case 3: this->readFunction = source_read3; break;
		case 4: this->readFunction = source_read4; break;
		default: this->readFunction = NULL;
	}

	*info = this->info;
	return this;
}

uint32_t source_read(Source this, uint8_t* dest) {
	return this->readFunction(this, dest);
}
uint32_t source_read1(Source this, uint8_t* dest) {
	return fread(dest, 1, this->info.height * this->info.width, this->fp);
}
uint32_t source_read3(Source this, uint8_t* dest) {
	uint32_t c = 0;
	for (size_t i = this->info.height; i; i--) {
		c += fread(this->buffer, this->info.colorScheme, this->info.width, this->fp);
		uint8_t* p = this->buffer;
		for (size_t j = this->info.width; j; j--) {
			*(dest++) = ( *(p++) + *(p++) + *(p++) ) / 3;
		}
	}
	return c;
}
uint32_t source_read4(Source this, uint8_t* dest) {
	uint32_t c = 0;
	for (size_t i = this->info.height; i; i--) {
		c += fread(this->buffer, this->info.colorScheme, this->info.width, this->fp);
		uint8_t* p = this->buffer;
		for (size_t j = this->info.width; j; j--) {
			*(dest++) = ( *(p++) + *(p++) + *(p++) ) / 3;
			p++;
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