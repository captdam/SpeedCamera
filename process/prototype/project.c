#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "project.h"

struct Project_ClassDataStructure {
	size2d_t cameraSize, marginSize, projectSize;
	float fovH, fovV;
	luma_t* edge;
	luma_t* buffer;
};

Project project_init(luma_t* edge, size2d_t cameraSize, float fovH, float fovV, float marginH, float marginV) {
	Project this = malloc(sizeof(struct Project_ClassDataStructure));
	if (!this)
		return NULL;
	
	this->marginSize.width =  cameraSize.width / fovH * marginH;
	this->marginSize.height = cameraSize.height / fovV * marginV;
	this->projectSize.width =  cameraSize.width + 2 * (this->marginSize).width;
	this->projectSize.height = cameraSize.height + 2 * (this->marginSize).height;
	
	this->buffer = malloc((this->projectSize).width * (this->projectSize).height * sizeof(luma_t));
	if (!(this->buffer))
		return NULL;
	
	this->edge = edge;
	this->cameraSize = cameraSize;
	this->fovH = fovH;
	this->fovV = fovV;
	return this;
}

void project_process(Project this, float yaw, float pitch) {
	luma_t* edge = this->edge;
	memset(this->buffer, 0, (this->projectSize).width * (this->projectSize).height);
	for (size_t imgY = 1; imgY < (this->cameraSize).height - 1; imgY++) {
		for (size_t imgX = 1; imgX < (this->cameraSize).width - 1; imgX++) {
			luma_t luma = *(edge++);

			//TODO: Use transformation matrix
			//TODO: Consider roll
			long int y = (this->marginSize).height + pitch / this->fovV * (this->cameraSize).height + imgY;
			long int x = (this->marginSize).width + yaw / this->fovH * (this->cameraSize).width + imgX;
			if ( y < 0 || y >= (this->projectSize).height || x < 0 || x >= (this->projectSize).width )
				continue; //Out of viewer-domain
			
			this->buffer[ y * (this->projectSize).width + x ] = luma;
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
	free(this->buffer);
	free(this);
}