/** This program is used to generate road map file and project file. 
 * A road map file contains infomation about the road-domain geographic data of each pixel. 
 * For example, if an object is located at a pixel at screen-domain (pixel-x-px, pixel-y-px). 
 * Using this data, we can get that object is on the road-domain (geo-x-meter, geo-y-meter). 
 * Format: array of struct {float geo-x-meter, float geo-y-meter, float-se} [height][width]. 
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <inttypes.h>
#include <errno.h>

typedef struct Point_t {
	double roadX, roadY;
	unsigned int screenX, screenY;
} point_t;

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
	if (argc != 11) {
		fputs(
			"Bad arg: Use 'this installHeight roadPitch installPitch fovHorizontal fovVertical sizeHorizontal sizeVeritcal threshold fileRoad fileProj'\n"
			"\twhere installHeight is height of camera in meter\n"
			"\t      roadPitch is pitch angle of road in degree, incline is positive\n"
			"\t      installPitch is pitch angle of camera in degree, looking sky is positive\n"
			"\t      fovHorizontal is horizontal field of view of camera in degree\n"
			"\t      fovVertical is vertical field of view of camera in degree\n"
			"\t      sizeHorizontal is width of camera image in unit of pixel\n"
			"\t      sizeVeritcal is height of camera image in unit of pixel\n"
			"\t      threshold is the max displacement an object can have between two frame in unit of meter\n"
			"\t      fileRoad is where to save the road map file\n"
			"\t      fileProj is where to save the project file\n"
			"\teg; this 10.0 20.0 0.0 57.5 32.3 1920 1080 3.5 map.data project.txt\n"
		, stderr);
		return EXIT_FAILURE;
	}
	double installHeight = atof(argv[1]), roadPitch = atof(argv[2]), installPitch = atof(argv[3]), fovHorizontal = atof(argv[4]), fovVertical = atof(argv[5]);
	unsigned int width = atoi(argv[6]), height = atoi(argv[7]);
	double threshold = atof(argv[8]);
	const char* roadmap = argv[9];
	const char* project = argv[10];
	fprintf(stdout, "Camera height: %lf m, pitch: %lf deg\n", installHeight, installPitch);
	fprintf(stdout, "Camera resolution: %u x %u p, FOV: %lf x%lf deg\n", width, height, fovHorizontal, fovVertical);
	fprintf(stdout, "Road picth: %lf deg\n", roadPitch);
	fprintf(stdout, "Threshold: %lf m/frame\n", threshold);
	fflush(stdout);

	FILE* fp;
	point_t farLeft, farRight, closeLeft, closeRight;
	
	fprintf(stdout, "Genarating road map file '%s'\n", roadmap);
	fflush(stdout);

	fp = fopen(roadmap, "wb");
	if (!fp) {
		fprintf(stderr, "Fail to create/open output file '%s' (errno=%d)\n", roadmap, errno);
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

	double lastY = 0.0;
	for (unsigned int y = 0; y < height; y++) {
		double cy = buffer[y][0][1]; //Current position; if above horizon, cy will be negative
		int isFar = lastY < 0 && cy > 0;
		int isClose = y == height - 1;
		for (unsigned int x = 0; x < width; x++) {
			double cx = buffer[y][x][0];

			if (isFar) {
				if (x == 0)
					farLeft = (point_t){.screenX = x, .screenY = y, .roadX = cx, .roadY = cy};
				else if (x == width - 1)
					farRight = (point_t){.screenX = x, .screenY = y, .roadX = cx, .roadY = cy};
			} else if (isClose) {
				if (x == 0)
					closeLeft = (point_t){.screenX = x, .screenY = y, .roadX = cx, .roadY = cy};
				else if (x == width - 1)
					closeRight = (point_t){.screenX = x, .screenY = y, .roadX = cx, .roadY = cy};;
			}
			
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

//			fprintf(stdout, "Point (%u,%u) - position %f %f, search region %f %f.\n", x, y, buffer[y][x][0], buffer[y][x][1], buffer[y][x][2], buffer[y][x][3]);
		}
		lastY = cy;
	}

	fwrite(buffer, sizeof(float), height * width * 4, fp);
	float max = threshold;
	fwrite(&max, sizeof(float), 1, fp);

	fputs("\n\n== Metadata: ===================================================================\n", fp);
	fprintf(fp, "CMD: %s %s %s %s %s %s %s %s\n", argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], argv[7], argv[8]);
	fprintf(fp, "Camera height: %lf m, pitch: %lf deg\n", installHeight, installPitch);
	fprintf(fp, "Camera resolution: %u x %u p, FOV: %lf x %lf deg\n", width, height, fovHorizontal, fovVertical);
	fprintf(fp, "Road picth: %lf deg\n", roadPitch);
	fprintf(fp, "Threshold: %lf m/frame\n", threshold);
	fclose(fp);

	fprintf(stdout, "Road map file '%s' generated\n", roadmap);
	fflush(stdout);

	fprintf(stdout, "Genarating project file '%s'\n", roadmap);
	fprintf(stdout, "Far left:    screen(%u, %u), road(%lf, %lf)\n", farLeft.screenX, farLeft.screenY, farLeft.roadX, farLeft.roadY);
	fprintf(stdout, "Far right:   screen(%u, %u), road(%lf, %lf)\n", farRight.screenX, farRight.screenY, farRight.roadX, farRight.roadY);
	fprintf(stdout, "Close left:  screen(%u, %u), road(%lf, %lf)\n", closeLeft.screenX, closeLeft.screenY, closeLeft.roadX, closeLeft.roadY);
	fprintf(stdout, "Close right: screen(%u, %u), road(%lf, %lf)\n", closeRight.screenX, closeRight.screenY, closeRight.roadX, closeRight.roadY);
	fflush(stdout);

	fp = fopen(project, "wb");
	if (!fp) {
		fprintf(stderr, "Fail to create/open output file '%s' (errno=%d)\n", project, errno);
		return EXIT_FAILURE;
	}

	double closeLeft2FarX = (closeLeft.roadX - farLeft.roadX) / (farRight.roadX - farLeft.roadX);
	double closeRight2FarX = (closeRight.roadX - farLeft.roadX) / (farRight.roadX - farLeft.roadX);
	double farY = farLeft.screenY / (double)height;
	/*                                      Proj-X			Proj-Y	Ortho-X	Orth-Y */
	fprintf(fp, "v\t%lf\t%lf\t%lf\t%lf\n",	closeLeft2FarX,		farY,	0.0,	farY); //LT
	fprintf(fp, "v\t%lf\t%lf\t%lf\t%lf\n",	closeRight2FarX,	farY,	1.0,	farY); //RT
	fprintf(fp, "v\t%lf\t%lf\t%lf\t%lf\n",	0.0,			1.0,	0.0,	1.0);  //LB
	fprintf(fp, "v\t%lf\t%lf\t%lf\t%lf\n",	1.0,			1.0,	1.0,	1.0);  //RB
	fputs("i\t0\t1\t2\n", fp);
	fputs("i\t1\t3\t2\n", fp);

	fclose(fp);

	fprintf(stdout, "Project file '%s' generated\n", project);
	fflush(stdout);

	fputs("Done!\n", stdout);
	return EXIT_SUCCESS;
}