/** This program is used to generate road map file. 
 * A road map file contains infomation about the road-domain geographic data of each pixel. 
 * For example, if an object is located at a pixel at screen-domain (pixel-x,pixely). 
 * Using this data, we can get that object is located at a point on the road (geo-a-meter,geo-b-meter). 
 * Format: 1-D plain array of struct {float geo-x, float geo-y} [size = width * height]. 
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <inttypes.h>
#include <errno.h>

double d2r(double deg) {
	return deg * M_PI / 180;
}

double r2d(double rad) {
	return rad * 180 / M_PI;
}

int main(int argc, char* argv[]) {
	if (argc != 9) {
		fputs(
			"Bad arg: Use 'this installHeight roadPitch installPitch fovHorizontal fovVertical sizeHorizontal sizeVeritcal file'\n"
			"\twhere installHeight is height of camera in meter\n"
			"\t      roadPitch is pitch angle of road in degree, incline is positive\n"
			"\t      installPitch is pitch angle of camera in degree, looking sky is positive\n"
			"\t      fovHorizontal is horizontal field of view of camera in degree\n"
			"\t      fovVertical is vertical field of view of camera in degree\n"
			"\t      sizeHorizontal is width of camera image in unit of pixel\n"
			"\t      sizeVeritcal is height of camera image in unit of pixel\n"
			"\teg; this 10.0 20.0 0.0 57.5 32.3 1920 1080 map.data\n"
		, stderr);
		return EXIT_FAILURE;
	}
	double installHeight = atof(argv[1]), roadPitch = atof(argv[2]), installPitch = atof(argv[3]), fovHorizontal = atof(argv[4]), fovVertical = atof(argv[5]);
	unsigned int width = atoi(argv[6]), height = atoi(argv[7]);
	const char* fn = argv[8];
	fprintf(stdout, "Camera height: %lf m, pitch: %lf deg\n", installHeight, installPitch);
	fprintf(stdout, "Camera resolution: %u x %u p, FOV: %lf x%lf deg\n", width, height, fovHorizontal, fovVertical);
	fprintf(stdout, "Road picth: %lf deg\n", roadPitch);
	fprintf(stdout, "Genarating roadmap to file '%s'\n", fn);
	fflush(stdout);

	FILE* fp = fopen(fn, "wb");
	if (!fp) {
		fprintf(stderr, "Fail to create/open output file '%s' (errno=%d)\n", fn, errno);
		return EXIT_FAILURE;
	}

	float data[2] = {0.0f, 0.0f};
	for (unsigned int y = 0; y < height; y++) {
		double lookPicth = installPitch - 0.5 * fovVertical + y / (height - 1.0) * fovVertical;
		double projY = installHeight / sin(d2r(lookPicth + roadPitch)) * sin(d2r(90 - lookPicth));
		data[1] = projY;
		
		double distance = installHeight / sin(d2r(lookPicth + roadPitch)) * sin(d2r(90 - roadPitch));
		for (unsigned int x = 0; x < width; x++) {
			double lookHorizontal = - 0.5 * fovHorizontal + x / (width - 1.0) * fovHorizontal;
			double projX = tan(d2r(lookHorizontal)) * distance;
			data[0] = projX;

			fwrite(data, sizeof(float), 2, fp);
		}
	}

	fclose(fp);
	fputs("Done!\n", stdout);
	return EXIT_SUCCESS;
}