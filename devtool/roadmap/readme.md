# Roadmap

A roadmap is an binary file that provides road-domain information associated with screen-domain pixels. This information is used to help the program to understant the scene in the video.

For example, if there is an active pixel at screen-domain position (x=30px, y=27px). The program can look up the roadmap, convert that position into road-domain location (x=3.5m, y=7.2m). Which means, there is an object at that geographic location.

## Terminology

The following terminologies are used in this document.

There are two domains when we describe the program and the roadmap: **Screen-domain** and **Road-domain**. Generally speaking, _computer_ "think" in screen-domain, it reads and draws pixels in screen-domain; while _human_ look the pixels on screen, but can realize objects in **Road-domain**. A better terminology is "world-domain", but for this program's purpose, since we only look objects on road, we will use "road-domain".

In screen-domain, we will always refer the top-left edge of the screen to (x=0px, y=0px), and refer the bottom-right edge to (x=```width```px, y=```height```px). Note this is different from OpenGL's normalized device coordinate (NDC).

In road-domain, we will always refer the orgin point (location of camera) to (x=0m, y=0m). We will always refer forward to +y axis and backward to -y axis; and always refer left to -x axis and right to +x axis.

When we refer the coordinate in this document, we will use (x,y), the first elemetn is the x-axis coordinate, the second element is y-axis coordinate. This document uses two ways to differentiate road-domain and screen-domain coordinates:

- (rx,ry) means road-domain coordinate; (sx,sy) means screen-domain coordinate. In this notation, "r" means road-domain; "s" means screen-domain. This notation is used when there is no actual coordinate value.

- (3.5m, 7.3m) means road-domain coordinate, (98px, 128px) means screen-doamin coordinate. In this notation, "m" (meter) is a road-domain unit, commonly used to discribe the distance of two geographic location; "px" (pixel) is a screen-domain uint, commonly used to describe the distance of two pixel on the screen. This notation is used when there is actual coordinate value. 

**Object**: Object means everything on road-domain. An object can be a car, a truck, or even a bird.

**Pixel**: Computer lacks the ability to realize objects. In computer's point of view, everything are pixels. Some objects takes one pixel, some objects can take multiple pixels.

**Location**: Location is used to describe where an object is. This terminology is used in _road-domain_. Somethime we use "geographic location" to exclusively state this terminology is for road-domain. An example is ```location (x=3.5m, y=7.2m)```, note the unit is in meter (m).

**Position**: Position is used to describe where a pixel is. This terminology is used in _screen-domain_. An example is ```position (x=30px, y=27px)```, note the unit is in pixel (px).

**Threshold**: For roadmap, the threshold means the maximum reasonable possible distance an object can travel during a given duration. Any object faster than the threshold will be ignored. This terminology is used in road-domain and have a unit of m/s.

**Search distance***: Similar to _threshold_, but in screen-domain. The "search distance" gives the maximum distance a pixel can move between two frames. Any pixel moving exceeding this value will be ignored, as the moving distance is greater than the program's search distance. The unit is pixel/frame.

**Video**: The video is a data stream feed to the program that contains the video of objects moving on the road. Source of video can be camera device, file, or anything that can feed a ```pipe```.

**Focus region**: A 2D mesh-defined area. The focus region enclose a screen-domain area of the video frame that SHOULD be processed by the program. Pixels out of the focus region MAY be ignored.

## Purpose of roadmap

### Focus region

In most case, road only occupies a portion of the video frame.

Instead of processing all pixels in the video, the program SHOULD only process pixels that are on the road. Doing so not only saves computation power by reducing number of workload pixels; but also eliminates noise outside of the road. For example, if there is a high-speed railway alongside a 50 km/h road, everytime a train passing by will trigger the speeding alarm without using focus region.

Therefore, the roadmap will provide the program a focus region. A focus region is a list of points on the screen-domain that defines the boundary of the road.

The focus region is not intended to be used by CPU. The program should construct this list into a mesh (technically speaking, a vertices array object, or VAO) and passing them to the GPU. During the fragment shader stage, the GPU will use hardware interpolation unit to determine whether a pixel is inside or outside of the focus region. The fragment shader will only process pixels in the focus region.

### Conversion between road-domain location and screen-roadm position

In real life, when we need to measure the speed of an object, we usually measure the change of geographic location of that object in a duration. Since ```speed = distance / time```, if an object travels 5 meters in 1 second, the speed of that object is 5 m/s, or 18 km/h.

However, for computer, there is no location but position. Computer cannot directly realize the distance an object travel in a duration in road-domain, all it see is that a pixel travels from position (sx1,sy1) to position (sx2,sy2) in the time of a frame in screen-domain. In order to find the road-domain distance the object travel, we need to convert the screen-domain starting position and ending position into road-domain locations.

The purpose of roadmap is to instruct the program to convert the two points, from screen-domain position (sx1,sy1) and (sx2,sy2) to road-domain location (rx1,ry1) and (rx2,ry2).

The last step is to perform calculation using the converted road-domain coordinates.

```
lookup(roadmap, (sx1, sy1)) => (rx1, ry1);
lookup(roadmap, (sx2, sy2)) => (rx2, ry2);
distance = sqrt( (rx1-rx2) ^ 2 + (ry1-ry2) ^ 2 );
speed = distance / frame;
speed = (distance / frame * FPS) / s
```

### Search distance

When we perform speed mesaurement of an object, we need to know the location of that object at the beginning and ending of the duration, which are location (rx1, ry1) and (rx2, ry2). In another word, to find the speed of an active pixel at position (sx1, sy1) at frame N, we need to know that pixel's position (sx2, sy2) at frame N+1. The location (rx2, ry2) should be close to (rx1, ry1); the position (sx1, sy1) should be close to (sx2, sy2). The greater the speed, the larger the distance.

In most case, the program should be able to find the postion (sx2,sy2) near the position (sx1, sy1). However, there are some exceptions that there may only be (sx1, sy1) or (sx2, sy2).

- At the moment of the object entering or exiting the focus region.

- The program fail to detect some objects for a short period.

- There is some object flashing in the video, such as traffic light.

- Noise.

In those case, the program is only able to find (sx1,sy1) or (sx2,sy2) but no another. Therefore, the program cannot determine the speed of that object. In fact, the program may search the entire video frame to try to find the other coordinate. This ends up wasting computation power, finding a wrong coordinate and produce wrong result.

To compensate this problem, we need to define a threshold of speed. This value defines the maximum possible distance an object could possibly travel during in one video frame. The program should stop searching when it cannot find an object in this threshold.

Since the threshold is a road-domain value, and the computer is working in screen-domain, we will need to comvert the threshold into search distance. The search distance is used to describe the maximum pixel distance an active pixel may travel during one video frame.

### Orthographic projection

In most case, a camera is placed on a pole near the road, facing toward the direction of traffic. The view mode is always in perspective view.

However, in perspective view, further object is smaller than closer object, and all projection lines vanished in the center point of the view plane. In another word, an object travel along the forwrd-backward direction in road-domain will not travel in the vertical direction in the screen-domain if it is not on the middle axis. This means, when the program tracing an object, it cannot simply tracing vertically. There will be a slope when tracing the object. Depending on how far the object is from the middle axis, the slope may become vary large.

One way is to add special care when tracing the object by adding the slope. The slope can be calculate at compile time, during program initialization, or on-the-fly by the GPU everytime a tracing is performed. Another way is to project the entire video into a semi-orthographic view (for simplicity, we will use orthographic view from now on without the semi prefix). In this orthographic view, further pixels are stretched horizontally, so their width will be the same as closer pixels. By doing this, object travel along the forwrd-backward direction in road-domain will now travel in the vertical direction in the screen-domain no matter how far it is from the middle axis. Now, when tracing objects, the program can simply look in the vertical direction without worry about the slope.

The orthographic projection only applies on the horizontal direction of the screen (x-axis). It is not practical to apply orthographic projection on the vertical direction of the screen (y-axis). There are a few reasons of applying orthographic projection only on the horizontal direction:

- Orthographic projection on the vertical direction is not necessary, it does not reduce compution complexity like the horizontal projection does.

- Because the furthest point located in the center of a perspctive view has infinite distance, it will require a infinite high framebuffer to store the vertically projected view if that furthest point need be included in the view. This can be solved by defining the focus region to not include the furthest points.

- Further objects are smaller, hence further pixels represent larger road-domain distance. In another word, they have a relatively low road-domain resolution compare to pixels representing closer road-domain points. If we apply projection on the vertical direction, the further portion will has less data density.

In conclusion, the orthographic projection should only stretch the video in horizontal direction. This means, the same point in perspective view and orthographic view will have same y-coordinate but different x-coordinate.

To perform the transformation from perspective view to orthographic view, we can either use a lookup table or a interpolated value in fragment shader. There is no absolute best choice, different machine may have different favor. We decide to use lookup table.

Although the program performs the computation in the orthographic view, when display the result, the result should be transformed back to perspective view.

## File format

The roadmap is a binary file. This file contains 5 segments of data.

### Segment 1: Header

The first segment of the roadmap file is the header. The following C code describe the header.

```C
typedef struct FileHeader {
	int16_t width, height;
	float searchThreshold;
	float orthoPixelWidth;
	unsigned int searchDistanceXOrtho;
} header_t;
```

The first field ```width, height``` contains 2 16-bit integers describing the dimention of the video frame and the size of this roadmap file. The video/image from the camera MUST match with this dimention; otherwise, error may occur.

The second field ```searchThreshold``` descibes the maximum distance an object can travel between two frames. This filed serves as a reference of how the search distance is calculated and can be ignored by the program.

The third field ```orthoPixelWidth``` describes the equivalent road-domain width of a pixel in orthographic view. All pixels in orthographic view will have same width. Note the height is different.

The fourth field ```searchDistanceXOrtho``` describes the horizontal (x-axis) search distance in orthographic view. Since all pixels in orthographic view have the same width, the horizontal search distance will be the same for all pixels. Note the vertical (y-axis) search distance is different.

Together the header takes 1 * (2+2) + 3 * 4 = 16 bytes.

### Segment 2: Table 1 - Road-domain geographic data

The second segment of the roadmap file is a table. This table is used to describe the road-domain geographic location associated with each pixel in both perspective view and orthographic view.

This table has ```height``` * ```width``` entires, where ```height``` and ```width``` are the dimention of the video frame. Each entries contains 4 floating-point numbers. Consider the following C code:

```C
typedef struct FileDataTable1 {
	float px, py;
	float ox, oy;
} data1_t;

int height = 1080, width = 1920;
data1_t data1[height][width];
```

For examples, if there is an active pixel on screen position (x=37px, y=29px) in the perspective view; that means, there is an object on road location (x=```data1[29][37].px```, y=```data1[29][37].py```).

This segment takes ```height``` * ```width``` * 4 * 4 bytes.

### Segment 3; Table 2 - Search distance & Conversion lookup

The third segment of the roadmap file is another table. This table contains search distance and perspective-orthographic conversion lookup of each pixel.

This table has ```height``` * ```width``` entires, where ```height``` and ```width``` are the dimention of the video frame. Each entries contains 4 unsigned integers. Consider the following C code:

```C
typedef struct FileDataTable2 {
	unsigned int searchDistanceXPersp;
	unsigned int searchDistanceY;
	unsigned int lookupXp2o, lookupXo2p;
} data2_t;

int height = 1080, width = 1920;
data2_t data2[height][width];
```

The first element ```searchDistanceXPersp``` donates the horizontal (x-axis) search distance in perspective view. Note that, in perspective view, further objects looks smaller; therefore, the equivalent road-domain width of each pixel is different. Because of this, we need to record ```searchDistanceXPersp``` for all pixels. On the other hand, in orthographic view, the equivalent road-domain width pixels are same; therefore, we only have one ```searchDistanceXOrtho```, and this value is saved in the header.

The second element ```searchDistanceY``` donates the vertical (y-axis) search distance. Since the orthographic view only transform the video in horizontal direction, both the orthographic view and the perspective view share the same ```searchDistanceY``` value.

For example, in orthographic view, if there is an active pixel at screen-domain position (x,y), the program should search the area defined by (x +/- ```table2[y][x].searchDistanceXOrtho```, y +/- ```table2[y][x].searchDistanceY```).

Theoretically speaking, in perspective view, if there is an active pixel at screen-domain position (x,y), the program should search the area defined by (x +/- ```table2[y][x].searchDistanceXPersp```, y +/- ```table2[y][x].searchDistanceY```). However, since the far-close axis in road-domain is not always the same as the y-axis in screen-domain, it is not practical to preform searching in perspective view.

The last two elements ```lookupXp2o``` and ```lookupXo2p``` are used to transform the video between orthographic view and the perspective view. The transfomation in only applied to horizontal direction; so, there is no ```lookupYp2o``` or ```lookupYo2p```.

For example, if a pixel at located at (x,y) in perspective view, it will be project to (```table2[y][x].lookupXp2o```, y) in orthographic view.

This segment takes ```height``` * ```width``` * 4 * 4 bytes.

### Segment 4: Focus region

The fourth segment of the roadmap file is a list of point pairs.

The first 4 bytes of this segment is an unsigned integer ```pCnt``` that gives the number of point pairs. Followed by a list of points in the following structure:

```C
typedef struct Point_t {
	float roadX, roadY;
	unsigned int screenX, screenY;
} point_t;

unsigned int pCnt = 10;
point_t points[pCnt][2];
```

We can consider a point to be a road-domain point, where its location is given by ```roadX``` and ```roadY```; we can also consider a point to be a screen-domain pixel, where its position is described by ```screenX``` and ```screenY```.

Road points are used to describe the screen-domain focus region, defines the area of pixel that should be processed by the program. Pixel outside of this area should be ignored, this method saves computation power and/or to ignores certain area that should be ignored.

In most case, road points are used to define the boundary of the road. Because of this, road points comes in pairs. ```points[i][0]``` is always the left point, ```points[i][1]``` is always the right point. Therefore, ```points[i][0].roadX``` and ```points[i][0].screenX``` is always less than ```points[i][1].roadX``` and ```points[i][1].screenX``` respectively.

In the road points list, the order is from further to closer in road-domain (top to bottom in screen-domain). Therefore, ```points[i][j].screenY``` is always less than ```points[i+N][j].screenY```; but ```points[i][j].roadY``` is always greater than ```points[i+N][j].roadY```.

This segment takes 4 + 2 * ```pCnt``` * 4 * 4 bytes.

## Segment 5: Meta data

The last segment stores human-readable data in ASCII format, such as comments, notes. The content of this segment is arbitrary and the length of this segment is arbitrary. This section SHOULD be ignored by the program.

## Size chart
- Seg 1: 16 bytes
- Seg 2: ```height``` * ```width``` * 4 * 4 bytes
- Seg 3: ```height``` * ```width``` * 4 * 4 bytes
- Seg 4: 4 + 2 * ```pCnt``` * 4 * 4 bytes
- Seg 5: DNC
- Total: (```height``` * ```width``` + ```pCnt```) * 32 + 20 bytes + DNC

## Consideration: Pre-calculated roadmap vs Run-time calculating based on camera orientation

One way to measure the speed is to use the pre-calculated roadmap as described in the earlier section of this document. Another way is to calculate the road-domain location on-the-fly using the orientation of the camera.

For this comparsing, we use an embedded system GPU, the VideoCore IV GPU archtecture which is used on Raspberry pi 3 (https://docs.broadcom.com/doc/12358545) as our reference. This comparsing is theory-base. Since GPU, GPU API and OpenGL are highly absturct, the comparsing may not be practical. Different GPU, different driver, different driver version, different optimization, different environment may produce different result.

### Pros of pre-calculated roadmap

- Only calculate constant data once, at compile-time.
- Avoid run-time computation-intense operation, which saves time and increases performance.
- Avoid using large amount of intermidiate value (cache)
- Support complex road (when the road-domain coordinates cannot be simply described by trigonometry).

### Cons of pre-calculated roadmap

- Uses more texture buffer, higher memory requirements.
- Intense use of texture and memory lookup unit (TMU).
- Lack of utilization of mathematical special function unit (SFU).

Note:
- Paspberry pi 4 is more main stream than PRI3, which uses VideoCore VI. But, there is no document available for this GPU. We believe that VideoCore VI may share some similarity with its predecessor videoCore IV, so we use the document of VideoCore IV for reference. However, it is also worth to pointed out that VideoCore VI supports openGL ES3.1; but its predecessor videoCore IV only supports OpenGL ES 2.x. Therefore, there may be some fundamental difference between them.
- According to the document, the VideoCore IV GPU may comes with 1 or 2 TMUs. GPUs with multiple TMUs are favored.

