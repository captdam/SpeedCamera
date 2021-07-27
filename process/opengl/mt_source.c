#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef VERBOSE
#include <errno.h>
#endif

#include "mt_source.h"

struct MultiThread_Source_ClassDataStructure {
	FILE* fp; //Inter-process communication with camera program
	size2d_t size; //Size of the video
	Fifo2 buffer; //Inter-thread communication between this reader thread and main thread
	pthread_t tid; //Source reading thread ID
	int tidValid; //tid valid? tid may be undefined (ref: man pthread). 0=invalid; 1=valid; -1=valid but request to end
};

void* mt_source_main(void* arg);

/* == Interface for main thread ============================================================= */

MT_Source mt_source_init(const char* filename) {
	#ifdef VERBOSE
		fputs("Init MT source class object\n", stdout);
		fflush(stdout);
	#endif

	/* Init class data */
	MT_Source this = malloc(sizeof(struct MultiThread_Source_ClassDataStructure));
	if (!this) {
		#ifdef VERBOSE
			fputs("Fail to create MT-Source class object data structure\n", stderr);
		#endif
		return NULL;
	}
	*this = (struct MultiThread_Source_ClassDataStructure){
		.fp = NULL,
		/*.tid = undefined, */
		.tidValid = 0,
		.buffer = NULL,
		.size = (size2d_t){0, 0}
	};

	/* Init FIFO */
/*	if (mkfifo(filename, 0777) == -1) {
		#ifdef VERBOSE
			fprintf(stderr, "Fail to create FIFO: %s (errno = %d)\n", filename, errno);
		#endif
		mt_source_destroy(this);
		return NULL;
	}*/
	this->fp = fopen(filename, "rb");
	if (!this->fp) {
		#ifdef VERBOSE
			fprintf(stderr, "Fail to open FIFO: %s (errno = %d)\n", filename, errno);
		#endif
		mt_source_destroy(this);
		return NULL;
	}
	fprintf(stdout, "FIFO (%s) ready, waiting for input data...\n", filename);
	fflush(stdout);

	/* Get and check video info header */
	vh_t info;
	if (!fread(&info, 1, sizeof(info), this->fp)) {
		#ifdef VERBOSE
			fputs("Cannot get video file info\n", stderr);
		#endif
		mt_source_destroy(this);
		return NULL;
	}
	fprintf(stdout, "Get video file header\tSize = %"PRIu16"*%"PRIu16" FPS=%"PRIu16", Color=%"PRIu16"\n", info.width, info.height, info.fps, info.colorScheme);
	fflush(stdout);

	if (info.colorScheme != 1) {
		#ifdef VERBOSE
			fputs("Bad color scheme (Accepts 1 only, using YUV but truncating UV)\n", stderr);
		#endif
		mt_source_destroy(this);
		return NULL;
	}
	if (info.fps != 0) {
		#ifdef VERBOSE
			fputs("Bad FPS (Accepts 0 only, expect timestamp in each frame data)\n", stderr);
		#endif
		mt_source_destroy(this);
		return NULL;
	}
	this->size = (size2d_t){.width = info.width, .height = info.height};

	/* Create double buffer for reading and processing */
	this->buffer = fifo2_init(this->size.width * this->size.height);
	if (!this->buffer) {
		#ifdef VERBOSE
			fputs("Fail to init inter-thread communication channel\n", stderr);
		#endif
		mt_source_destroy(this);
		return NULL;
	}

	/* Init reader thread */
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	int source_error = pthread_create(&this->tid, &attr, mt_source_main, this);
	if (source_error) {
		#ifdef VERBOSE
			fprintf(stderr, "Fail init Source thread (%d)\n", source_error);
		#endif
		mt_source_destroy(this);
		return NULL;
	}
	this->tidValid = 1;

	#ifdef VERBOSE
		fputs("A new thread is created for video frame reading\n", stderr);
		fflush(stdout);
	#endif
	return this;
}

size2d_t mt_source_getSize(MT_Source this) {
	return this->size;
}

void* mt_source_start(MT_Source this) {
	if (this->tidValid == -1)
		return NULL;
	return fifo2_read_start(this->buffer);
}

void mt_source_finish(MT_Source this) {
	fifo2_read_finish(this->buffer);
}

void mt_source_destroy(MT_Source this) {
	if (!this)
		return;

	#ifdef VERBOSE
		fputs("Destroy MT-Source class object\n", stdout);
		fflush(stdout);
	#endif

	if (this->tidValid) {//Kill the thread first, in case the thread access any resource before the cancellation point
		pthread_cancel(this->tid);
		pthread_join(this->tid, NULL);
		this->tidValid = 0;
		#ifdef VERBOSE
			fputs("Source thread killed\n", stdout);
			fflush(stdout);
		#endif
	}

	if (this->fp)
		fclose(this->fp);
	fifo2_destroy(this->buffer);
}

/* == Routines for reading thread, private, internal ======================================== */

#ifdef VERBOSE
void mt_source_cancelLog(void* arg) {
	fputs("[MT-Source] Video frame reading thread cancelled!\n", stderr);
	fflush(stdout);
}
#endif

void* mt_source_main(void* arg) {
	#ifdef VERBOSE
		pthread_cleanup_push(&mt_source_cancelLog, NULL);
		fputs("[MT-Source] Video frame reading thread start!\n", stderr);
		fflush(stdout);
	#endif
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

	MT_Source this = arg;

	for (;;) {
		void* wptr = fifo2_write_start(this->buffer);
		if (!fread(wptr, 1, this->size.width * this->size.height, this->fp)) { //Read, detect EOF or broken pipe
			#ifdef VERBOSE
				fputs("[MT-Source] End of file, or broken pipe!\n", stdout);
				fflush(stdout);
			#endif
			this->tidValid = -1; //Request the main thread to kill the reader thread. We leave the control to main thread.
		}
		fifo2_write_finish(this->buffer);
	}

	#ifdef VERBOSE
		pthread_cleanup_pop(1);
	#endif
	return NULL;
}
