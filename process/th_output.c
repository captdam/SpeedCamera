#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

#include "th_output.h"

int _valid = 0;
int p[2] = {0, 0};
pthread_t tid; //Reader thread ID

struct itc_header {
	int frame;
	int count;
};

void* th_output(void* arg);

int th_output_init() {
	if (pipe(p) == -1) {
		fprintf(stderr, "Fail to create inter thread communication between main thread and output thread\n");
		return 0;
	}

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	int err = pthread_create(&tid, &attr, th_output, NULL);
	if (err) {
		fprintf(stderr, "Fail to create output thread: %d\n", err);
		close(p[0]); p[0] = 0;
		close(p[1]); p[1] = 0;
		return 0;
	}

	_valid = 1;
	return 1;
}

void th_output_write(const int frame, const int count, output_data* data) {
	output_header header = {
		.frame = frame,
		.count = count
	};
	!write(p[1], &header, sizeof(output_header));
	!write(p[1], data, count * sizeof(output_data));
}

void th_output_destroy() {
	if (!_valid)
		return;
	_valid = 0;

	close(p[0]); p[0] = 0;
	close(p[1]); p[1] = 0;
	
	pthread_cancel(tid);
	pthread_join(tid, NULL);
}

void* th_output(void* arg) {
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

	output_header header;
	for(;;) {
		!read(p[0], &header, sizeof(output_header));
		if (header.count == -1)
			break;
		
		output_data data[header.count];
		!read(p[0], data, header.count * sizeof(output_data));

		fprintf(stdout, "F %u : %u\n", header.frame, header.count);
		for (output_data* ptr = data; ptr < data + header.count; ptr++) {
			fprintf(stdout, "O S %d : R %.2f,%.2f : S %u,%u : dy %d\n", ptr->speed, ptr->rx, ptr->ry, ptr->sx, ptr->sy, ptr->osy);
		}
	}

	return NULL;
}