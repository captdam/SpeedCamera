/** Speed Camera (function verifying version)
 * Command: ./this inputBitmapVideoFileName width height fps
 * This program will demonstrate how this algorithm detect object speed in the video without using
 * radar or other types of external device.
 * 
 * ====== NOT FOR PRODUCTION ======
 * This program is used to verify and demonstrate the functionality of this algorithm.
 * There are tons and tons of computation on each pixel, CPU is too slow and GPU is too expensive.
 * The solution: FPGA. Relatively cheap, reprogrammable, low power consumption. Suitable for edge device.
 */

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

/**************************************** Config ****************************************/

// Data structure in input file can be in RGB or RGBA format
// Using RGBA can align data in 32-bit word, hence take advantage of vector instruction.
// Using RGB saves 25% of space, hence take adcantage of cache.
// By default, this program will use RGB.
// To use RGBA, uncomment the following line.
//#define RGBA;

// Save the edge detection kernel filtered bitmap video
// This is for debugging purpose. Writing file to disk is SLOW!
// To save the edge detection kernel filtered bitmap video, define the output file name in the following line.
// To skip this costy and slow operation, comment the following line.
#define OUTPUT_FILTERED "./kfilter.data"

// Using floating-point number or signed int type for filter
// The mask and filter operation can be performed in float or int.
// Edge detection normally uses int type, bluring normally uses float type.
// It is fine to use float type for edge detection, but it is really SLOW and unnecessary.
// How about double and float128, absolutely wasting of power
// Note: Image data is saved in uint8_t. If we are going to use int, we will need int16 or larger data type
// Here we will use "int", which is the native signed int type for the machine determined by compiler
typedef int kfilter_t; //int or float

/****************************************  End~  ****************************************/

//Input file data structure
typedef struct PixelDatatype {
	uint8_t r;
	uint8_t g;
	uint8_t b;
#ifdef RGBA
	uint8_t a;
#endif /* #ifdef RGBA */
} pixel_t;

void kernelFilter(
	size_t filterWidth, size_t filterHeight,
	size_t imageWidth, size_t imageHeight,
	kfilter_t filter[filterHeight][filterWidth],
	pixel_t inputImage[imageHeight][imageWidth], uint8_t filteredImage[imageHeight][imageWidth]
) {
	size_t edgeLeft = filterWidth >> 1, edgeRight = imageWidth - (filterWidth >> 1) - 1;
	size_t edgeTop = filterHeight >> 1, edgeBottom = imageHeight - (filterHeight >> 1) - 1;
	for (size_t y = 0; y < imageHeight; y++) {
		for (size_t x = 0; x < imageWidth; x++) {
			if ( x < edgeLeft || x > edgeRight || y < edgeTop || y > edgeBottom ) {
				filteredImage[y][x] = 0;
			} else {
//				printf("Process pixel (y,x) = (%zu,%zu)\n", y, x);
				//Apply filter - Accumulate
				kfilter_t luma = 0;
				for (size_t fy = 0; fy < filterHeight; fy++) {
					for (size_t fx = 0; fx < filterWidth; fx++) {
						size_t srcy = y + fy - (filterHeight>>1), srcx = x + fx - (filterWidth>>1);
						luma += (inputImage[srcy][srcx]).r * filter[fy][fx];
						luma += (inputImage[srcy][srcx]).g * filter[fy][fx];
						luma += (inputImage[srcy][srcx]).b * filter[fy][fx];
//						printf("\tRead (%zu,%zu), rgb=%03d/%03d/%03d (C=%03d/%03d/%03d)\tfilter=%d\n", srcy, srcx, inputImage[srcy][srcx].r, inputImage[srcy][srcx].g, inputImage[srcy][srcx].b, rgb[0], rgb[1], rgb[2], (int)filter[fy][fx]);
					}
				}
				//Apply filter - clamp
				luma /= 3;
				if (luma > 255)
					filteredImage[y][x] = 0xFF;
				else if (luma < 0)
					filteredImage[y][x] = 0x00;
				else
					filteredImage[y][x] = luma;
//				printf("Apply, luma=%03d\n", luma);
			}
			
		}
	}
}

int main(int argc, char* argv[]) {

	/* Program command */
	if (argc < 5) {
		fputs("Bad usage. Try command: ./this inputFilename width height fps\n", stderr);
		exit(EXIT_FAILURE);
	}
	const char* inputFilename = argv[1];
	const size_t width = atoi(argv[2]);
	const size_t height = atoi(argv[3]);
	const unsigned int fps = atoi(argv[4]);

	/* Heap allocation */
	size_t inputFrameSize = sizeof(pixel_t) * height * width; //See struct PixelDatatype
	size_t kFilteredFrameSize = sizeof(uint8_t) * height * width; //Gray 8-bit
	void* buffer = malloc( inputFrameSize + kFilteredFrameSize );
//	pixel_t (*inputFrame)[width] =		buffer;
//	uint8_t (*kFilteredFrame)[width] =	buffer + inputFrameSize;
	pixel_t* inputFrame =		buffer;
	uint8_t* kFilteredFrame =	buffer + inputFrameSize;
	
	/* Open video source file */
	FILE* inputFile = fopen(inputFilename, "rb");
#ifdef OUTPUT_FILTERED
	FILE* kFilteredFile = fopen("./kfilter.data", "wb");
#endif /* #ifdef OUTPUT_FILTERED */
	
	/* Edge detection */
	kfilter_t f[3][3] = { //Must be odd by odd
		{-1, -1, -1},
		{-1, 8, -1},
		{-1, -1, -1}
	};
	while (fread(inputFrame, inputFrameSize, 1, inputFile)) {
		kernelFilter(sizeof(f) / sizeof(f[0]), sizeof(f[0]) / sizeof(f[0][0]), width, height, f, (pixel_t(*)[])inputFrame, (uint8_t(*)[])kFilteredFrame);
#ifdef OUTPUT_FILTERED
		fwrite(kFilteredFrame, kFilteredFrameSize, 1, kFilteredFile);
#endif /* #ifdef OUTPUT_FILTERED */
	}
	
	/* Done, clean up */
	fclose(inputFile);
#ifdef OUTPUT_FILTERED
	fclose(kFilteredFile);
#endif /* #ifdef OUTPUT_FILTERED */
	free(buffer);
	exit(EXIT_SUCCESS);
}

