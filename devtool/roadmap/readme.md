# Roadmap Generator

Used to generate road map used in comparing step of the main program.

Calculating the distance between pixels (road points) on-the-fly comsumes a lot computation power. Since the distance between road points will never change, we can pre-calculate the distance, save it in a table. When we need it, we just read the value from the table without calculating it.

## Terminology

**Road point**: A road point is a point on the road that can be projected on the screen-domain. A camera of x-by-y resolution will have x * y road points.

**Neighbor point**: A neighbor point is another road point geographically close to a road point. A road point may have multiple neighbor points.

## How to use

First, configure the program base on the user scene by modifying the source code.

The first part, the 8 define values tells the program information about the size of the camera/video, its field of view, the position of the camera and the measuring range.

The second part tells the program the geographical coordnate (x,y,z in meter) of each road point associated with a pixel on the screen. In this example, we assume the road is perfectly flat. The user can modifty this program to fit their need. It is absolutely OK if the user want to use a look-up table for an uneven road or any road condition that cannot (or hard to be) represented by equations.

The third part tells the program which region of the frame image should be focus and which region of the frame image should be ignored. For example, we need to focus only the road portion of the video, and ignore the greenland beside the road. Reducing the focus region gives large advantage in terms of processing speed and memory usage.

```C
/* == Modify following values and functions to generate road map file for a specific scene == */

//Screen resolution in pixel
#define WIDTH 1920
#define HEIGHT 1080

//Camera field of view (Horizontal and Veritcal) in degree
#define FOVH 57.5
#define FOVV 32.3

//Position of camera: height in meter, pitch in degree, 0 = horizontal, 90 = sky, -90 = ground
#define INSTALL_HEIGHT 10.0
#define INSTALL_PITCH -14.0

//Max speed of object in km/h, Video FPS. Thes two value also used as threshold for searching neighbor points
#define MAX_SPEED 120.0
#define FPS 30.0

/** Road location generation
 * @param x screen coordnate x in pixel of a road point
 * @param y screen coordnate y in pixel of a road point
 * @return x,y,z geographical coordnate of a road point
 */
loc3d_t getLocation(int x, int y) {
	double pitchTop = (90.0 + INSTALL_PITCH + 0.5*FOVV) * M_PI / 180;
	double pitchBottom = (90.0 + INSTALL_PITCH - 0.5*FOVV) * M_PI / 180;
	double pitchStep = (pitchBottom - pitchTop) / (HEIGHT - 1);
	double pitchCurrent = pitchTop + pitchStep * y;

	double yawSpan = INSTALL_HEIGHT / cos(pitchCurrent) * sin(0.5 * FOVH * M_PI / 180); //Half
	double yawStep = 2 * yawSpan / (WIDTH - 1);
	double yawCurrent = -yawSpan + yawStep * x;

	return (loc3d_t){
		.x = yawCurrent,
		.y = tan(pitchCurrent) * INSTALL_HEIGHT,
		.z = 0
	};
}

/** Focus region, movement outside of the focus region will be ignored
 * @param x screen coordnate x in pixel of a road point
 * @param y screen coordnate y in pixel of a road point
 * @return 1 if in focus region , 0 if out
 */
int isFocused(int x, int y) {
	int x1, x2, y1, y2;

	if (y < 400 || y > 1060)
		return 0;
	
	x1 = 940;
	y1 = 400;
	x2 = 410;
	y2 = 1060;
	if ( x < x1 + (y-y1) * (x1-x2)/(y1-y2) )
		return 0;
	
	x1 = 1060;
	y1 = 400;
	x2 = 1496;
	y2 = 656;
	if ( x > x1 + (y-y1) * (x1-x2)/(y1-y2) )
		return 0;
	
	if (x > 1496)
		return 0;

	return 1;
}

/* == Modify above values and functions to generate road map file for a specific scene == */
```

Then, compile with this command:
```gcc main.c -lm -O3```

...and excute with:
```./a.out some_place/whatever_you_want_to_call_it.data```

This will generate the required roadmap file for the main program at ```some_place/whatever_you_want_to_call_it.data```.

If the scene changed, you will need to **modify the source code** mentioned above, and **re-compile** and **re-run** this generator program using the above commands.

## What does this file do & How it work

This section is for information purpose, actual implementation may be different.

### What to look when detect movement

This file will tell the program which pixel to look when it detects a movement on a pixel.

This file contains a list of road points and a list of neighbor points.

For a camera of x-by-y resolution, there will be x * y road points. The road points data sturcture tells the program which block of the neighbor list to look.

For a camera of x-by-y resolution, there will be have x * y blocks of neighbor points. Each block contains a number of individual neighbor points (Note that, the number may be 0 for some blocks). Each neighbor point contains two parameters: distance between this neighbor point to its associated road point, and the position of this neighbor point (the position is represented by index of the negihbor points in the frame, which is an 1D array with size of x * y). The neighbor points in each block is sorted by distance from low to high.

### How it work

When the program detects a movement on pixel(x,y) in the screen, it first check the road points list for road(x,y). This returns address to the neighbor points block associated with road(x,y) in the memory, and the length of that block.

Next, the program will read the neighbor points. The neighbor points can be found by using the address given by road(x,y) from the road points list.

The neighbor points conatin the position of the checking pixel and its geographically distance. There are a number of neighbor points need to be check.

The program strats from the first neighbor point, which is the closest to the road point. If the pixel pointed by this position indicates that the object is presented in the last frame, the program stops here and returns the distance. This means, the object is at that position in last frame; so, the object moved for this much distance during the frame interval.

If the object is not presented in the pixel pointed by first neighbor point, the program goes to check the pixel pointed by the second neighbor point, which represents the second-closed point. If not, the program moved to the 3rd neighbor point and so on.

If the program checked all neighbor points associated witch this road point, but cannot find any result, this program returns 0 distance. This means the object moves too fast, it moves more than the threshold distance between the frame interval. This either means the movement is noise, or the object moves too fast. This a edge case, we simply ignore this.

### Why threshold?

We cannot check the entire frame for each road point. It is too much work. If we have a 1920*1080 frame, we need to check 4.2e12 times each frame in worst case scenario (O(n^2)).

When we generate this file, we need to know the FPS (interval between the frame) and the max possible speed of our object. For example, if we are measuring the speed of car, we are sure there is no road-car can be faster than 250km/h. In this case, we only check neighbor points that is within 2.32 meter range from the road point if we are working with 30 FPS video.

```
250 km/h = 69.44 m/s
69.44m/s / 30frame/s = 2.31 m/frame
```

By doing so, we can limit the number of neighbor points to check; hence reduce compuation need and memory need.

## File format

The map file contains 3 parts.

First of all, we assume all our data can be addressed by using 32-bit pointer. Also, we want to implement our program on 32-bit low-end embedded device. Therefore, we will use ```uint32_t```, ```int32_t``` insteadof ```size_t``` to reduce file size.

### Part A - Total size of neighbor points

We need to record the total number of neighbor points of all road points before we can start reading the neighbor points list. This is because we need to allocate memory before we read it into memory. We are using C, not Python or other types of languages supporting dynamic data type and array size.

We only need to record the size of neighbor points list (shown in Part C). The size of road points list (shown in Part B) can be calculated by simply multiply width, heigh and size of ```road_t```.

```C
return WIDTH * HEIGHT * sizeof(road_t);
```

### Part B - Road points list

The neighbor points list is a big plain array of ```neighbor_t```, which contains all neighbor points of all road points.

Plain means there is no boundary between two neighbor points blocks of two road points. Just like there is no boundary between two sub-arraies of a 2D array in C.

We need to tell the program which block of the neighbor points list should it look for a specific road point. We do this by assign each road point a ```base address``` and ```count```. Consider the following data type in C:
```C
struct road_t {
	uint32_t base;
	uint32_t count;
};
```
where:
- Base means the base index (it IS base INDEX, NOT base ADDRESS) of this pixel.
- Count means the number of neighbor points of this road point.

This part (Part B) of this file is an array of ```road_t```, each represents one road point. The order is from left-to-right, then top-to-bottom.

So, let's assume that the width of our frame is 1920. Then, for road point located at (x=1400, y=960), we need to check the following neighbors:
```C
uint32_t x = 1400, y = 960, width = 1920;
road_t* roads = init();
neighbor_t* neighbors = init();

uint32_t startIndex = roads[ y * width + x ].base;
uint32_t endIndex = startIndex + roads[ y * width + x ].count;
for (uint32_t i = startIndex; i < endIndex; i++) {
	do_something(neighbors[i]);
}
```

### Part C - Neighbor points list

Finally, the core part. This part contains a list of neighbor points.

A neighbor point is a 32-bit structure, it has a 8-bit ```distnace``` and a 24-bit ```pos```. Where:
- **Distance** is the displacement from the neighbor point to its associated road point. To represent the distance using 8-bit unsigned int, this value is scaled by using equation ```geographical_distance / threshold * 255```
- **Pos** means The position of this neighbor point. This value is same as the offset (index) of the pointed road point in the frame buffer.

There are width * height neighbor points blocks. Each neighbor points blcok contains a number of individual neighbor points. That start index and length of each neighbor points block can be found in the road point list mentioned in part B.

### File structure example

```
Address	(byte)		Content				Description										Ref
==========================================================================================================================================================
00000000		(uint32)total_count		Total number of neighbot of all pixel (used to hint memory allocator), see ref-C
----------------------------------------------------------------------------------------------------------------------------------------------------------
00000004		(uint32)pixel_0_0_base		Base index of pixel(x0,y0)'s neighbor map, the first pixel, top-left , always 0 for p(0,0), see Ref-A
00000008		(uint32)pixel_0_0_count		Number of neighbors of pixel(x0,y0)
0000000C		(uint32)pixel_1_0_base		Base index of pixel(x1,y0)'s neighbor map, the second pixel, top-2nd-left, see Ref-B
00000010		(uint32)pixel_1_0_count		Number of neighbors of pixel(x1,y0)
...			...				...
width*height*8 - 0004	(uint32)pixel_w-1_h-1_base	Base index of pixel(xw-1,yh-1)'s neighbor map, the last pixel, bottom-right
width*height*8 - 0000	(uint32)pixel_w-1_h-1_count	Number of neighbors of pixel(xw-1,yh-1)
----------------------------------------------------------------------------------------------------------------------------------------------------------
width*height*8 + 0004	(uint8)pixel_0_0_distance_1	Distance from point 1 to pixel(x0,y0); 0 = 0, 255 = distance threshold			Ref-A
	= width*height*8 + 0004 + pixel_0_0_count + 0000
width*height*8 + 0005	(uint24)pixel_0_0_pos1		Address of point 1; point1.y * width + point1.x
	= width*height*8 + 0004 + pixel_0_0_count + 0001
width*height*8 + 0008	(uint8)pixel_0_0_distance_2	Distance from point 2 to pixel(x0,y0)
	= width*height*8 + 0004 + pixel_0_0_count + 0004
width*height*8 + 0009	(uint24)pixel_0_0_pos2		Address of point 2
	= width*height*8 + 0004 + pixel_0_0_count + 0005
width*height*8 + 000C	(uint8)pixel_0_0_distance_3	Distance from point 3 to pixel(x0,y0)
	= width*height*8 + 0004 + pixel_0_0_count + 0008
width*height*8 + 000D	(uint24)pixel_0_0_pos3		Address of point 3
	= width*height*8 + 0004 + pixel_0_0_count + 0009
...			...				...
width*height*8 + 0004 + pixel_1_0_base*4													Ref-B
	= width*height*8 + 0004 + pixel_0_0_count*4
			(struct32)pixel_1_0_distance_and_pos_1
							Distance and position of point 1 to pixel(x1,y1)
width*height*8 + 0004 + pixel_1_0_base*4 + 0004
			(struct32)pixel_1_0_distance_and_pos_1
							Distance and position of point 2 to pixel(x1,y1)
width*height*8 + 0004 + pixel_1_0_base*4 + 0008
			(struct32)pixel_1_0_distance_and_pos_2
...			...				...
----------------------------------------------------------------------------------------------------------------------------------------------------------
width*height*8 + 0004 + total_count*4			End of file (EOF), this address is not included in the file					Ref-C
```