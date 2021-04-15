#include <stdlib.h>
#include <math.h>

//#include "project.h"

struct Project_ClassDataStructure {
	size_t imageWidth, imageHeight;
	size_t projectWidth, projectHeight;
	size_t bufferHorizontal, bufferVertical;
	float fovHorizontal, fovVertical;
	luma_t* luma;
};

typedef struct Project_ClassDataStructure* Project;

Project project_init(size_t width, size_t height, float fovH, float fovV, float bufferH, float bufferV) {
	Project this = malloc(sizeof(struct Project_ClassDataStructure));
	if (!this)
		return NULL;
	
	this->bufferHorizontal = width / fovH * bufferH;
	this->bufferVertical = height / fovV * bufferV;
	this->projectWidth = width + 2 * this->bufferHorizontal;
	this->projectHeight = height + 2 * this->bufferVertical;
	
	this->luma = calloc(this->projectWidth * this->projectHeight, sizeof(luma_t));
	if (!(this->luma))
		return NULL;
	
	this->imageWidth = width;
	this->imageHeight = height;
	this->fovHorizontal = fovH;
	this->fovVertical = fovV;
	return this;
}

luma_t* project_getProjectLuma(Project this) {
	return this->luma;
}

void project_getProjectSize(Project this, size_t* w, size_t* h) {
	*w = this->projectWidth;
	*h = this->projectHeight;
}

void project_write(Project this, float yaw, float pitch, luma_t* edge) {
	for (size_t imgY = 1; imgY < this->imageHeight - 1; imgY++) {
		for (size_t imgX = 1; imgX < this->imageWidth - 1; imgX++) {
			luma_t luma = *(edge++);

			//TODO: Use transformation matrix
			//TODO: Consider roll
			long int y = this->bufferVertical + pitch / this->fovVertical * this->projectHeight + imgY;
			long int x = this->bufferHorizontal + yaw / this->fovHorizontal * this->projectWidth + imgX;
			if (y < 0 || y >= this->projectHeight || x < 0 || x >= this->projectWidth)
				continue; //Out of projection-domain field
			
			this->luma[ y * this->projectWidth + x ] = luma;
		}
		
	}
}

void project_destroy(Project this) {
	free(this->luma);
	free(this);
}