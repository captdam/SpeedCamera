#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>

#ifdef VERBOSE
#include <errno.h>
#endif

#include "mt_source.h"

struct validTID {
	int valid;
	pthread_t tid; //tid valid? tid may be undefined (ref: man pthread). 0=invalid; 1=valid; -1=valid but request to end
};

struct MultiThread_Source_ClassDataStructure {
	FILE* fp; //Inter-process communication with camera program
	vh_t info; //Size of the video frame, fps and color scheme
	char type; //Input file type (c for camera, f for FIFO, \0 for regular file)
	Fifo2 buffer; //Inter-thread communication between this reader thread and main thread
	struct validTID threadRead; //Reader thread
	pid_t processCam; //Camera process
};

const char* fifoname = "./tempframefifo.data";

void* mt_source_main(void* arg);

/* == Interface for main thread ============================================================= */

MT_Source mt_source_init(const char* filename) {
	const char* dir;

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
		.info = (vh_t){0, 0, 0, 0},
		.type = '\0',
		.buffer = NULL,
		.threadRead = (struct validTID){.valid=0},
		.processCam = 0
	};

	/* Source type and file name */
	if (*filename == 'c' || *filename == 'f') {
		this->type = *filename;
		dir = fifoname;
	}
	else {
		this->type = '\0';
		dir = filename;
	}
	#ifdef VERBOSE
		fputs("Init MT source class object.\t", stdout);
		if (this->type == 'c')
			fprintf(stdout, "Use Pi Camera. Temp FIFO name: %s\n", dir);
		else if (this->type == 'f')
			fprintf(stdout, "Type FIFO. FIFO name: %s\n", dir);
		else if (this->type == '\0')
			fprintf(stdout, "Type file. File name: %s\n", dir);
		fflush(stdout);
	#endif

	/* Init FIFO */
	// (c) Create a FIFO so the camera process can use it
	// (f) Create a FIFO to accept data from other types of external source
	// (0) File aleady exist, no need to create a FIFO
	if (this->type == 'c' || this->type == 'f') {
		if (mkfifo(dir, 0777) == -1) {
			#ifdef VERBOSE
				fprintf(stderr, "Fail to create source FIFO (errno = %d)\n", errno);
			#endif
			mt_source_destroy(this);
			return NULL;
		}
	}

	/* Init source process */
	// (c) Start the camera process
	// (f) Waiting the user to start the external process amnually (no action on this program)
	// (0) File aleady exist, directly read it (no action on this program)
	if (this->type == 'c') {
		pid_t pid = fork();
		if (pid == -1) {
			#ifdef VERBOSE
				fprintf(stderr, "Fail to folk child process (errno = %d)\n", errno);
			#endif
			mt_source_destroy(this);
			return NULL;
		}
		if (pid == 0) { //In child process
			FILE* fp = fopen(dir, "wb");
			if (!fp) {
				#ifdef VERBOSE
					fprintf(stderr, "Fail to open camera interface (errno = %d)\n", errno);
				#endif
				mt_source_destroy(this);
				return NULL;
			}
			vh_t info = {.width = 1280, .height = 720, .fps = 20, .colorScheme = 1};
			fwrite(&info, 1, sizeof(info), fp);
			fflush(fp);
			/* fclose(fp); Do NOT close the file descriptor to keep the pipe open. We didn't put lock, so other process can write the file */

			char* arg[] = {
				"raspividyuv",
				"-w","1280", "-h","720", "-fps","20", "-y", //1280*720, 20 FPS, luma only
				"-t","0", //infinite time
				"--preview","700,50,640,360", //Display preview window
//				"--nopreview", //Or no preview
				"-o",(char*)dir, //Output dest
				NULL
			};
			if (execvp(arg[0], arg) == -1) {
				#ifdef VERBOSE
					fprintf(stderr, "Fail to start child process (raspividyuv) (errno = %d)\n", errno);
				#endif
				mt_source_destroy(this);
				return NULL;
			}

			exit(0);
		}
		this->processCam = pid;
		fputs("Camera process started, waiting for camera init...\n", stdout);
		fflush(stdout);
	}
	else if (this->type == 'f') {
		fprintf(stdout, "FIFO ready, dir = %s, waiting for input data...\n", dir);
		fflush(stdout);
	}

	/* Open video file */
	this->fp = fopen(dir, "rb");
	if (!this->fp) {
		#ifdef VERBOSE
			fprintf(stderr, "Fail to open source interface (errno = %d)\n", errno);
		#endif
		mt_source_destroy(this);
		return NULL;
	}

	/* Get and check video info header */
	if (!fread(&this->info, 1, sizeof(this->info), this->fp)) {
		#ifdef VERBOSE
			fputs("Cannot get video file info\n", stderr);
		#endif
		mt_source_destroy(this);
		return NULL;
	}
	size2d_t size = {.width = this->info.width, .height = this->info.height};
	fprintf(stdout, "Get video file header\tSize = %zu*%zu FPS=%"PRIu16", Color=%"PRIu16"\n", size.width, size.height, this->info.fps, this->info.colorScheme);
	fflush(stdout);

	if (this->info.colorScheme > 4) {
		#ifdef VERBOSE
			fputs("Bad color scheme. Accepts 1 (Y, gray), 2(RGB565), 3(RGB8) and 4(RGBA8)\n", stderr);
		#endif
		mt_source_destroy(this);
		return NULL;
	}

	/* Create double buffer for reading and processing */
	this->buffer = fifo2_init(size.width * size.height);
	if (!this->buffer) {
		#ifdef VERBOSE
			fputs("Fail to init inter-thread communication channel\n", stderr);
		#endif
		mt_source_destroy(this);
		return NULL;
	}

	/* Init reader thread */ {
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
		int source_error = pthread_create(&this->threadRead.tid, &attr, mt_source_main, this);
		if (source_error) {
			#ifdef VERBOSE
				fprintf(stderr, "Fail init Source thread (%d)\n", source_error);
			#endif
			mt_source_destroy(this);
			return NULL;
		}
		this->threadRead.valid = 1;
	}

	#ifdef VERBOSE
		fputs("A new thread is created for video frame reading\n", stderr);
		fflush(stdout);
	#endif
	return this;
}

vh_t mt_source_getInfo(MT_Source this) {
	return this->info;
}

void* mt_source_start(MT_Source this) {
	if (this->threadRead.valid == -1)
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

	if (this->type == 'c') { //Kill the camera process
		kill(this->processCam, SIGINT);
		#ifdef VERBOSE
			fputs("Kill camera process\n", stdout);
			fflush(stdout);
		#endif
	}

	if (this->threadRead.valid == -1) { //Kill the thread thread, this stops any resource access
		pthread_cancel(this->threadRead.tid);
		pthread_join(this->threadRead.tid, NULL);
		this->threadRead.valid = 0;
		#ifdef VERBOSE
			fputs("Source thread killed\n", stdout);
			fflush(stdout);
		#endif
	}

	if (this->fp) //Close the file
		fclose(this->fp);

	if (this->type == 'c' || this->type == 'f') //And remove the file if it is a temp fifo
		unlink(fifoname);

	fifo2_destroy(this->buffer); //Last step, destroy the inter thread communication

	free(this);
}

/* == Routines for reading thread, private, internal ======================================== */

#ifdef VERBOSE
void mt_source_main_cancelLog(void* arg) {
	fputs("[MT-Source] Video frame reading thread cancelled!\n", stderr);
	fflush(stdout);
}
#endif

void* mt_source_main(void* arg) {
	#ifdef VERBOSE
		pthread_cleanup_push(&mt_source_main_cancelLog, NULL);
		fputs("[MT-Source-Main] Video frame reading thread start!\n", stderr);
		fflush(stdout);
	#endif
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

	MT_Source this = arg;

	for (;;) {
		size_t length = (size_t)this->info.width * (size_t)this->info.height;
		uint8_t* wptr = fifo2_write_start(this->buffer);
		if (this->info.colorScheme == 1) {
			if (!fread(wptr, 1, length, this->fp)) { //Read, detect EOF or broken pipe
				#ifdef VERBOSE
					fputs("[MT-Source] End of file, or broken pipe!\n", stdout);
					fflush(stdout);
				#endif
				this->threadRead.valid = -1; //Request the main thread to kill the reader thread. We leave the control to main thread.
			}
		}
		else if (this->info.colorScheme == 2) {
			for (size_t i = length; i; i--) {
				uint16_t temp;
				if (!fread(&temp, 2, sizeof(uint8_t), this->fp)) {
					#ifdef VERBOSE
						fputs("[MT-Source] End of file, or broken pipe!\n", stdout);
						fflush(stdout);
					#endif
					this->threadRead.valid = -1;
				}
				else {
					uint8_t r = (temp >>  0) & 0b00011111;
					uint8_t g = (temp >>  5) & 0b00111111;
					uint8_t b = (temp >> 11) & 0b00011111;
					*wptr++ = (r + g + b) << 1; //r+g+b max = 31+63+31=125
				}
			}
		}
		else if (this->info.colorScheme == 3) {
			for (size_t i = length; i; i--) {
				uint8_t temp[3];
				if (!fread(temp, 3, sizeof(uint8_t), this->fp)) {
					#ifdef VERBOSE
						fputs("[MT-Source] End of file, or broken pipe!\n", stdout);
						fflush(stdout);
					#endif
					this->threadRead.valid = -1;
				}
				else {
					uint8_t r = temp[0] >> 2;
					uint8_t g = temp[1] >> 2;
					uint8_t b = temp[3] >> 2;
					*wptr++ = r + g + b;
				}
			}
		}
		else if (this->info.colorScheme == 4) {
			for (size_t i = length; i; i--) {
				uint8_t temp[4];
				if (!fread(temp, 4, sizeof(uint8_t), this->fp)) {
					#ifdef VERBOSE
						fputs("[MT-Source] End of file, or broken pipe!\n", stdout);
						fflush(stdout);
					#endif
					this->threadRead.valid = -1;
				}
				else {
					uint8_t r = temp[0] >> 2;
					uint8_t g = temp[1] >> 2;
					uint8_t b = temp[3] >> 2;
					*wptr++ = r + g + b;
				}
			}
		}
		
		fifo2_write_finish(this->buffer);
	}

	#ifdef VERBOSE
		pthread_cleanup_pop(1);
	#endif
	return NULL;
}