#include <stdlib.h>
#include <string.h>

#include <stdio.h>

#include "project.h"

struct Project_ClassDataStructure {
	luma_t* input;
	luma_t* buffer;
	size2d_t inputSize, marginSize, projectSize;
	float fovH, fovV;
};

Project project_init(luma_t* input, size2d_t inputSize, float fovH, float fovV, float marginH, float marginV) {
	Project this = malloc(sizeof(struct Project_ClassDataStructure));
	if (!this)
		return NULL;
	
	this->input = input;
	this->buffer = NULL;
	this->inputSize = inputSize;
	this->marginSize.width =  inputSize.width / fovH * marginH;
	this->marginSize.height = inputSize.height / fovV * marginV;
	this->projectSize.width =  inputSize.width + 2 * this->marginSize.width;
	this->projectSize.height = inputSize.height + 2 * this->marginSize.height;
	this->fovH = fovH;
	this->fovV = fovV;
	
	this->buffer = malloc(this->projectSize.width * this->projectSize.height * sizeof(luma_t));
	if (!(this->buffer)) {
		project_destroy(this);
		return NULL;
	}
	
	return this;
}

void project_process(Project this, float yaw, float pitch, float roll) {
	luma_t* input = this->input;
	memset(this->buffer, 0, this->projectSize.width * this->projectSize.height);

	int offsetY = this->marginSize.height + pitch / this->fovV * this->inputSize.height; //May be nagative, int is large enough, even for AVR's 16-bit int (anyone get video wider than 32k?)
	int offsetX = this->marginSize.width + yaw / this->fovH * this->inputSize.width;

	for (int y = offsetY; y < (int)this->inputSize.height + offsetY; y++) {
		if (y < 0) { //Higher than upper boundary, input advance for one line
			input += this->inputSize.width;
		}
		else if (y > this->projectSize.height) { //Lower than bottom boundary, end
			break;
		}
		else {
			for (int x = offsetX; x < (int)this->inputSize.width + offsetX; x++) {
				if (x < 0) {
					input++;
				}
				else if (x > this->projectSize.width) {
					input++;
				}
				else {
					this->buffer[y * this->projectSize.width + x] = *(input++);
				}
			}
		}
	}
}

size2d_t project_getProjectSize(Project this) {
	return this->projectSize;
}

luma_t* project_getProjectImage(Project this) {
	return this->buffer;
}

void project_destroy(Project this) {
	if (!this)
		return;

	free(this->buffer);
	free(this);
}