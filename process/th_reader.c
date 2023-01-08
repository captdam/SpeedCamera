#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "th_reader.h"

#define BLOCKSIZE 64 //Height and width are multiple of 8, so frame size is multiple of 64. Read a block (64px) at once can increase performance
#define FIFONAME "tmpframefifo.data" //Video input stream

#define reader_info(format, ...) {fprintf(stderr, "[Reader] Log:\t"format"\n" __VA_OPT__(,) __VA_ARGS__);} //Write log
#define reader_error(format, ...) {fprintf(stderr, "[Reader] Err:\t"format"\n" __VA_OPT__(,) __VA_ARGS__);} //Write error log

void* th_reader(void* arg); //Reader thread private function - reader thread
int th_reader_readLuma(); //Reader thread private function - read luma video
int th_reader_readRGB(); //Reader thread private function - read RGB video
int th_reader_readRGBA(); //Reader thread private function - read RGBA video
int th_reader_readRGBADirect();//Reader thread private function - read RGBA video with channel = RGBA (in order)

int valid = 0; //Thread has been init successfully
pthread_t tid; //Reader thread ID
sem_t sem_readerStart; //Fired by main thread: when pointer to pbo is ready, reader can begin to upload
sem_t sem_readerDone; //Fired by reader thread: when uploading is done, main thread can use
volatile void volatile* rawDataPtr; //Video raw data goes here. Main thread write pointer here, reader thread put data into this address
struct { unsigned int r, g, b, a; } colorChannel; //Private, for reader read function
unsigned int blockCnt; //Private, for reader read function
FILE* fp; //Private, for reader read function

int th_reader_init(const unsigned int size, const char* colorScheme, char** statue, int* ecode) {
	if (sem_init(&sem_readerStart, 0, 0)) {
		if (ecode)
			*ecode = errno;
		if (statue)
			*statue = "Fail to create main-reader master semaphore";
		return 0;
	}

	if (sem_init(&sem_readerDone, 0, 0)) {
		if (ecode)
			*ecode = errno;
		if (statue)
			*statue = "Fail to create main-reader secondary semaphore";
		sem_destroy(&sem_readerStart);
		return 0;
	}

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	struct th_reader_arg arg = {.size = size, .colorScheme = colorScheme};
	int err = pthread_create(&tid, &attr, th_reader, &arg);
	if (err) {
		if (ecode)
			*ecode = err;
		if (statue)
			*statue = "Fail to create thread";
		sem_destroy(&sem_readerDone);
		sem_destroy(&sem_readerStart);
		return 0;
	}
	sem_wait(&sem_readerStart); //Wait for the thread start. Prevent this function return before thread ready

	valid = 1;
	return 1;
}

void th_reader_start(void* addr) {
	rawDataPtr = addr;
	sem_post(&sem_readerStart);
}

void th_reader_wait() {
	sem_wait(&sem_readerDone);
}

void th_reader_destroy() {
	if (!valid)
		return;
	valid = 0;
	
	pthread_cancel(tid);
	pthread_join(tid, NULL);

	sem_destroy(&sem_readerDone);
	sem_destroy(&sem_readerStart);
}

void* th_reader(void* arg) {
	struct th_reader_arg* this = arg;
	unsigned int size = this->size;
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
	int (*readFunction)();

	reader_info("Frame size: %u pixels. Number of color channel: %c", size, this->colorScheme[0]);

	if (this->colorScheme[1] != '\0') {
		colorChannel.r = this->colorScheme[1] - '0';
		if (this->colorScheme[2] != '\0') {
			colorChannel.g = this->colorScheme[2] - '0';
			if (this->colorScheme[3] != '\0') {
				colorChannel.b = this->colorScheme[3] - '0';
				if (this->colorScheme[4] != '\0')
					colorChannel.a = this->colorScheme[4] - '0';
			}
		}
	}
	if (this->colorScheme[0] == '1') {
		reader_info("Color scheme: RGB = idx0, A = 0xFF");
		blockCnt = size / BLOCKSIZE;
		readFunction = th_reader_readLuma;
	} else if (this->colorScheme[0] == '3') {
		reader_info("Color scheme: R = idx%d, G = idx%d, B = idx%d, A = 0xFF", colorChannel.r, colorChannel.g, colorChannel.b);
		blockCnt = size / BLOCKSIZE;
		readFunction = th_reader_readRGB;
	} else {
		reader_info("Color scheme: R = idx%d, G = idx%d, B = idx%d, A = idx%d", colorChannel.r, colorChannel.g, colorChannel.b, colorChannel.a);
		if (colorChannel.r != 0 || colorChannel.g != 1 || colorChannel.b != 2 || colorChannel.a != 3) {
			blockCnt = size / BLOCKSIZE;
			readFunction = th_reader_readRGBA;
		} else {
			readFunction = th_reader_readRGBADirect;
			blockCnt = size;
		}
	}

	unlink(FIFONAME); //Delete if exist
	if (mkfifo(FIFONAME, 0777) == -1) {
		reader_error("Fail to create FIFO '"FIFONAME"' (errno = %d)", errno);
		return NULL;
	}

	reader_info("Ready. FIFO '"FIFONAME"' can accept frame data now"); //Ready
	sem_post(&sem_readerStart); //Unblock the main thread

	fp = fopen(FIFONAME, "rb"); //Stall until some data writed into FIFO
	if (!fp) {
		reader_error("Fail to open FIFO '"FIFONAME"' (errno = %d)", errno);
		unlink(FIFONAME);
		return NULL;
	}
	reader_info("FIFO '"FIFONAME"' data received");

	int cont = 1;
	while (cont) {
		sem_wait(&sem_readerStart); //Wait until main thread issue new memory address for next frame

		if (!readFunction())
			cont = 0;

		#ifdef DEBUG_THREADSPEED
			debug_threadSpeed = 'R';
		#endif
		
		sem_post(&sem_readerDone); //Uploading done, allow main thread to use it
	}

	fclose(fp);
	unlink(FIFONAME);
	reader_info("End of file or broken pipe! Please close the viewer window to terminate the program");

	while (1) { //Send dummy data to keep the main thread running
		sem_wait(&sem_readerStart); //Wait until main thread issue new memory address for next frame
		
		memset((void*)rawDataPtr, 0, size * 4);

		#ifdef DEBUG_THREADSPEED
			debug_threadSpeed = 'R';
		#endif

		sem_post(&sem_readerDone); //Uploading done, allow main thread to use it
	}

	return NULL;
}

int th_reader_readLuma() {
	uint8_t* dest = (uint8_t*)rawDataPtr;
	uint8_t luma[BLOCKSIZE];
	for (unsigned int i = blockCnt; i; i--) { //Block count = frame size / block size
		if (!fread(luma, 1, BLOCKSIZE, fp)) {
			return 0; //Fail to read, or end of file
		}
		for (uint8_t* p = luma; p < luma + BLOCKSIZE; p++) {
			*(dest++) = *p; //R
			*(dest++) = *p; //G
			*(dest++) = *p; //B
			*(dest++) = 0xFF; //A
		}
	}
	return 1;
}

int th_reader_readRGB() {
	uint8_t* dest = (uint8_t*)rawDataPtr;
	uint8_t rgb[BLOCKSIZE * 3];
	for (unsigned int i = blockCnt; i; i--) {
		if (!fread(rgb, 3, BLOCKSIZE, fp)) {
			return 0;
		}
		for (uint8_t* p = rgb; p < rgb + BLOCKSIZE * 3; p += 3) {
			*(dest++) = p[colorChannel.r]; //R
			*(dest++) = p[colorChannel.g]; //G
			*(dest++) = p[colorChannel.b]; //B
			*(dest++) = 0xFF; //A
		}
	}
	return 1;
}

int th_reader_readRGBA() {
	uint8_t* dest = (uint8_t*)rawDataPtr;
	uint8_t rgba[BLOCKSIZE * 4];
	for (unsigned int i = blockCnt; i; i--) {
		if (!fread(rgba, 4, BLOCKSIZE, fp)) {
			return 0;
		}
		for (uint8_t* p = rgba; p < rgba + BLOCKSIZE * 4; p += 4) {
			*(dest++) = p[colorChannel.r]; //R
			*(dest++) = p[colorChannel.g]; //G
			*(dest++) = p[colorChannel.b]; //B
			*(dest++) = p[colorChannel.a]; //A
		}
	}
	return 1;
}

int th_reader_readRGBADirect() {
	if (!fread((void*)rawDataPtr, 4, blockCnt, fp)) //Block count = frame size / block size
		return 0;
	return 1;
}