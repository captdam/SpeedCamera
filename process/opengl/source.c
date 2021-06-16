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
};

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

	this->buffer = malloc(this->info.width * this->info.height * this->info.colorScheme);
	if (!this->buffer) {
		#ifdef VERBOSE
			fputs("\tCannot allocate buffer for file reader interface\n", stderr);
		#endif

		source_destroy(this);
		return NULL;
	}

	*info = this->info;
	return this;
}

size_t source_read(Source this, gl_obj dest) {
	size_t l = fread(this->buffer, 1, this->info.width * this->info.height * this->info.colorScheme, this->fp);
	gl_updateTexture(&dest, this->info, this->buffer);
	return l;
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