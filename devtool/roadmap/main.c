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

double fd(double x1, double y1, double x2, double y2) {
	double dx = x1 - x2, dy = y1 - y2;
	return sqrt(dx * dx + dy * dy);
}

int main(int argc, char* argv[]) {
	if (argc != 10) {
		fputs(
			"Bad arg: Use 'this installHeight roadPitch installPitch fovHorizontal fovVertical sizeHorizontal sizeVeritcal threshold file'\n"
			"\twhere installHeight is height of camera in meter\n"
			"\t      roadPitch is pitch angle of road in degree, incline is positive\n"
			"\t      installPitch is pitch angle of camera in degree, looking sky is positive\n"
			"\t      fovHorizontal is horizontal field of view of camera in degree\n"
			"\t      fovVertical is vertical field of view of camera in degree\n"
			"\t      sizeHorizontal is width of camera image in unit of pixel\n"
			"\t      sizeVeritcal is height of camera image in unit of pixel\n"
			"\t      threshold is the max displacement an object can have between two frame in unit of meter\n"
			"\t      file is where to save the roadmap file\n"
			"\teg; this 10.0 20.0 0.0 57.5 32.3 1920 1080 3.5 map.data\n"
		, stderr);
		return EXIT_FAILURE;
	}
	double installHeight = atof(argv[1]), roadPitch = atof(argv[2]), installPitch = atof(argv[3]), fovHorizontal = atof(argv[4]), fovVertical = atof(argv[5]);
	unsigned int width = atoi(argv[6]), height = atoi(argv[7]);
	double threshold = atof(argv[8]);
	const char* fn = argv[9];
	fprintf(stdout, "Camera height: %lf m, pitch: %lf deg\n", installHeight, installPitch);
	fprintf(stdout, "Camera resolution: %u x %u p, FOV: %lf x%lf deg\n", width, height, fovHorizontal, fovVertical);
	fprintf(stdout, "Road picth: %lf deg\n", roadPitch);
	fprintf(stdout, "Threshold: %lf m/frame\n", threshold);
	fprintf(stdout, "Genarating roadmap to file '%s'\n", fn);
	fflush(stdout);

	FILE* fp = fopen(fn, "wb");
	if (!fp) {
		fprintf(stderr, "Fail to create/open output file '%s' (errno=%d)\n", fn, errno);
		return EXIT_FAILURE;
	}

	float (* buffer)[width][4] = malloc(sizeof(float) * height * width * 4);
	if (!buffer) {
		fprintf(stderr, "Out of memory (errno=%d)\n", errno);
		fclose(fp);
		return EXIT_FAILURE;
	}

	for (unsigned int y = 0; y < height; y++) {
		double lookPicth = installPitch - 0.5 * fovVertical + y / (height - 1.0) * fovVertical;
		double projY = installHeight / sin(d2r(lookPicth + roadPitch)) * sin(d2r(90 - lookPicth));
		
		double d = installHeight / sin(d2r(lookPicth + roadPitch)) * sin(d2r(90 - roadPitch));
		for (unsigned int x = 0; x < width; x++) {
			double lookHorizontal = - 0.5 * fovHorizontal + x / (width - 1.0) * fovHorizontal;
			double projX = tan(d2r(lookHorizontal)) * d;

			buffer[y][x][0] = projX;
			buffer[y][x][1] = projY;
		}
	}

	for (unsigned int y = 0; y < height; y++) {
		for (unsigned int x = 0; x < width; x++) {
			double cx = buffer[y][x][0], cy = buffer[y][x][1]; //Current position
			
			unsigned int left = 0;
			while (1) {
				if (left > x) break; //Out of screen
				unsigned int ix = x - left, iy = y; //Target index
				if (fd(cx, cy, buffer[iy][ix][0], buffer[iy][ix][1]) > threshold) break; //Distance from current position to target greater than threshold
				left++;
			}

			unsigned int right = 0;
			while (1) {
				if (right + x >= width) break;
				unsigned int ix = x + right, iy = y;
				if (fd(cx, cy, buffer[iy][ix][0], buffer[iy][ix][1]) > threshold) break;
				right++;
			}

			unsigned int up = 0;
			while(1) {
				if (up > y) break;
				unsigned int ix = x, iy = y - up;
				if (fd(cx, cy, buffer[iy][ix][0], buffer[iy][ix][1]) > threshold) break;
				up++;
			}

			unsigned int down = 0;
			while(1) {
				if (down + y >= height) break;
				unsigned int ix = x, iy = y + down;
				if (fd(cx, cy, buffer[iy][ix][0], buffer[iy][ix][1]) > threshold) break;
				down++;
			}

			buffer[y][x][2] = fmax(left, right);
			buffer[y][x][3] = fmax(up, down);

	//		fprintf(stdout, "Point (%u,%u) - position %f %f, search region %f %f.\n", x, y, buffer[y][x][0], buffer[y][x][1], buffer[y][x][2], buffer[y][x][3]);
		}
	}

	fwrite(buffer, sizeof(float), height * width * 4, fp);
	float max = threshold;
	fwrite(&max, sizeof(float), 1, fp);

	fputs("\n\n== Metadata: ===================================================================\n", fp);
	fprintf(fp, "Camera height: %lf m, pitch: %lf deg\n", installHeight, installPitch);
	fprintf(fp, "Camera resolution: %u x %u p, FOV: %lf x %lf deg\n", width, height, fovHorizontal, fovVertical);
	fprintf(fp, "Road picth: %lf deg\n", roadPitch);
	fprintf(fp, "Threshold: %lf m/frame\n", threshold);
	fprintf(fp, "Genarating roadmap to file '%s'\n", fn);

	fclose(fp);
	fputs("Done!\n", stdout);
	return EXIT_SUCCESS;
}