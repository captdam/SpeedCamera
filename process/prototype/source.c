#include <stdlib.h>
#include <stdio.h>

#ifdef VERBOSE
#include <errno.h>
#endif

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
#ifdef VERBOSE
	fputs("Init source class object\n", stdout);
	fflush(stdout);
#endif

	Source this = malloc(sizeof(struct Source_ClassDataStructure));
	if (!this) {
#ifdef VERBOSE
		fputs("\tFail to create source class object data structure\n", stderr);
#endif
		return NULL;
	}

	this->fp = NULL;
	this->buffer = NULL;
	
	this->fp = fopen(videoFile, "rb");
	if (!this->fp) {
#ifdef VERBOSE
		fprintf(stderr, "\tFail to open input file %s (errno = %d)\n", videoFile, errno);
#endif
		source_destroy(this);
		return NULL;
	}

	if (!fread(&this->info, 1, sizeof(this->info), this->fp)) {
#ifdef VERBOSE
		fputs("\tCannot get video info\n", stderr);
#endif
		source_destroy(this);
		return NULL;
	}

#ifdef VERBOSE
	fprintf(stdout, "\tVideo info: w=%"PRIu16",  h=%"PRIu16",  color=%"PRIu16",  fps=%"PRIu16"\n", this->info.width, this->info.height, this->info.colorScheme, this->info.fps);
	fflush(stdout);
#endif

	this->buffer = malloc(this->info.width * this->info.colorScheme);
	if (!this->buffer) {
#ifdef VERBOSE
		fputs("\tCannot allocate buffer for file reader interface\n", stderr);
#endif
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

#ifdef VERBOSE
	fputs("Destroy source class object\n", stdout);
	fflush(stdout);
#endif
	if (this->fp) fclose(this->fp); //fclose(NULL) is undefined
	free(this->buffer);
	free(this);
}