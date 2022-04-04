# Roadmap

A roadmap is a binary file that provides road-domain information associated with screen-domain pixels. 

This information is used to help the program to understand the scene in the video. There are four purposes of using the roadmap. 

First, the roadmap defines a region called "focus region". The focus region encircles a region in the video frame that the program should process. Everything out of the region can be ignored. 

Second, the roadmap gives the associated road-domain location for each screen-domain position. For example, if there is an active pixel at screen-domain position (e.g. x=30px, y=27px). The program can look up the roadmap, convert that position into road-domain location (e.g. x=3.5m, y=7.2m). Which means, there is an object at that geographic location. 

Third, the roadmap defines the search limit. A search limit is maximum distance an object could reasonably travel. While searching, the program should stop when it reaches the limit. 

Last, the roadmap provides a lookup table used to transform the video between orthographic and perspective view. 

## Terminology

The following terminologies are used in this document.

There are two domains when we describe the program and the roadmap: **Screen-domain** and **Road-domain**. Generally speaking, _computer_ "think" in screen-domain, it reads and draws pixels in screen-domain; while _human_ look the pixels on screen but can realize objects in **Road-domain**. A better terminology is "world-domain", but for this program's purpose, since we are studying objects on road, we will use "road-domain". 

In screen-domain, there are three types of coordinate systems: 

- The first one is **absolute** pixel system, where we will always refer the top-left corner of the screen to (x=0px, y=0px), and refer the bottom-right corner to (x=```width```px, y=```height```px), where ```width``` and ```height``` is the size of the video in pixels. This system is used for index when we need to find a specific data in the roadmap.

- The second one is **normalized device coordinate, or NDC**, which is used in OpenGL vertices. We will always refer the bottom-left corner to (-1.0, -1.0), and refer the top-right corner to (1.0, 1.0). This system is not used in roadmap. 

- The third one is **normalized texture coordinate, or NTC**, which is used in OpenGL texture. We will always refer the top-left corner to (0.0, 0.0), and refer the bottom-right corner to (1.0, 1.0). All data in roadmap uses this system.

In road-domain, we will always refer the origin point (anchor location of the camera pole) to (x=0m, y=0m). We will always refer forward to +y axis and backward to -y axis; and always refer left to -x axis and right to +x axis. 

When we refer to the coordinate in this document, we will use (x,y), the first element is the x-axis coordinate, the second element is y-axis coordinate. This document uses two ways to differentiate road-domain and screen-domain coordinates: 

- (rx,ry) means road-domain coordinate; (sx,sy) means screen-domain coordinate. In this notation, "r" means road-domain; "s" means screen-domain. This notation is used when there is no actual coordinate value. 

- (3.5m, 7.3m) means road-domain coordinate, (98px, 128px) means screen-domain coordinate. In this notation, "m" (meter) is a road-domain unit, commonly used to describe the geographic location; "px" (pixel) is a screen-domain unit, commonly used to describe the pixel on the screen. This notation is used when there is actual coordinate value. If there is no unit, like (0.5, 0.8), it means screen-domain NTC. 

**Object**: Object means everything on road-domain. An object can be a car, a truck, or even a deer. 

**Pixel**: Computer lacks the ability to realize objects. In computer's point of view, everything are pixels. Some objects take one pixel, some objects can take multiple pixels. 

**Location**: Location is used to describe where an object is. This terminology is used in _road-domain_. Sometimes we use "geographic location" to exclusively state this terminology is for road-domain. An example is ```location (x=3.5m, y=7.2m)```, note the unit is in meter (m). 

**Position**: Position is used to describe where a pixel is. This terminology is used in _screen-domain_. Sometimes we use "screen position" to exclusively state this terminology is for screen-domain. Examples are ```position (x=30px, y=27px)``` and ```(x=0.3, y=0.95)```, note the unit in pixel (px) or no unit. 

**Threshold**: For roadmap, the threshold means the maximum reasonable distance an object can travel during a given duration. Any object faster than the threshold will be ignored. This terminology is used in road-domain and has a unit of m/s. 

**Search distance***: Similar to _threshold_, but in screen-domain. The "search distance" gives the maximum distance a pixel can move between two frames. Any pixel moving exceeding this value will be ignored, because the moving distance is greater than the program's search distance. The unit is pixel/frame. 

**Video**: The video is a data stream feed to the program that contains the video of objects moving on the road. Source of video can be camera device, or a file, or anything that can feed a ```pipe```. 

**Focus region**: A 2D mesh defined area. The focus region encircles a screen-domain area in the video frame that SHOULD be processed by the program. Pixels out of the focus region MAY be ignored. 

## Purpose of roadmap

### Focus region

In most cases, the road only occupies a portion of the video frame. 

Instead of processing all pixels in the video, the program should only process pixels that are on the road. Doing so not only saves computation power by reducing the number of workload pixels; but also eliminates noise outside of the road. For example, if there is a high-speed railway alongside a 50 km/h road, without using focus region, every time a train passing by will trigger the speeding alarm. 

Therefore, the roadmap will provide the program a focus region. A focus region is a list of points on the screen-domain that defines the boundary of the road. 

![Focus region](docs/focusRegion.jpeg "Focus region")

This image illustrates the purpose of the focus region. In this image, the road takes less than 1/3 of the entire image. By ignoring the area outside the red box, the workload could be dramatically reduced. 

The focus region is not intended to be processed by CPU. The program should construct this list into a mesh (technically speaking, a vertices array object, or VAO) and pass them to the GPU. During the fragment shader stage, the GPU will connect all road points to form a polygon. The GPU automatically determine whether or not a pixel is inside of the focus region. The fragment shader will only process pixels in the focus region.

### Conversion between road-domain location and screen-roadm position

In real life, when we need to measure the speed of an object, we usually measure the change of geographic location of that object in a duration. Since ```speed = distance / time```. Fox example, if an object travels 5 meters in 1 second, the speed of that object is 5 m/s, or 18 km/h. 

However, for computers, there is no location but screen position. Computer cannot directly realize the distance an object travels in a duration in road-domain. All it sees is that a grounp of pixels travels from position (sx1,sy1) to position (sx2,sy2) in the time of a frame. In order to find the road-domain distance the object travels, we need to convert the screen-domain starting position and ending position into road-domain locations.

#### Lookup table

There is a table in roadmap that gives the program all the necessary information to convert screen-domain position (sx1,sy1) and (sx2,sy2) to road-domain location (rx1,ry1) and (rx2,ry2). 

After conversion, the program can perform calculation using the converted road-domain coordinates. 

```
lookup(roadmap, (sx1, sy1)) => (rx1, ry1);
lookup(roadmap, (sx2, sy2)) => (rx2, ry2);
distance = sqrt( (rx1-rx2) ^ 2 + (ry1-ry2) ^ 2 );
speed = distance / frame;
speed = (distance / frame * FPS) / s
```

#### Satellite image

There are multiple ways to generate the lookup table. One way is to use satellite image.

First, we need to take a screenshot of the image provided by the camera. We also need to find a satellite image that shows the same scene. We can use Google map for this task.

Then, we need to find a few key points (screen points) on the screenshot, record its screen-domain NTC. The more key points, the better the result.

Next, we associate those key points from the screenshot to points to the satellite image, to create associated road points. We then measure the road-domain geographic location of each road points.

![Satellite](docs/satellite.jpg "Satellite")

Finally, we can use software to interpolate the road information to form a roadmap.

#### Geometry

Another way is to use geometry.

![Trigonometry](docs/trigonometry.png "Trigonometry") 

In this method, we will need to use the camera height, camera angle of view (AOV), position of the object in camera screen, and road pitch to calculate the distance of the object from the camera pole. In the above figure, the camera pitch is the angle between the center line of the camera and the camera pole; therefore, the angle between the upper edge of camera view plane (screen) and the angle of between the lower edge of the camera view plane will be ```CameraPitch + 0.5 * AOV``` and ```CameraPitch - 0.5 * AOV``` respectively. 

To find the distance between the object and the camera pole, we will need to draw a triangle that connects three points: the camera, the object, and the camera pole anchor on the ground. Consider the following figure: 

![Trigonometry Example](docs/triEx.png "Trigonometry Example") 

First, we need to find ∠A, the angle between the object and camera pole. When there is an object in the scene, we need to get the NTC y-coord of that object ```y```. If this object is at the upper edge of camera view plane, y will be 0.0; if this object is at the lower edge of camera view plane, y will be 1.0; if this object is at somewhere middle of camera view plane, y will be between 0.0 and 1.0. Then, ∠A will be ```CameraPitch + 0.5 * AOV - y * AOV```. 

Next, we need to find ∠B, the angle between the camera pole and the road surface. We can assume the camera pole is perfectly vertical, perpendicular to the ground level. Then, ∠B will be ```90 - RoadPitch```. ```RoadPitch``` is the slope of the road, this data can be measured when installing this system.  

We will also need to know the ```CameraHeight```. This data can be measured when installing this system. 

In triangle with angle ∠A, ∠B, ∠C and line a, b c, according to sine rule:
```
Sin(A) / a = Sin(B) / b = Sin(C) / c
```
Therfore:
```
Sin(A) / ObjectDistance = Sin(C) / CameraHeight
A = CameraPitch + 0.5 * AOV - y * AOV
B = 90 - RoadPitch
C = 180 - A - B = 90 + RoadPitch + y * AOV - CameraPitch - 0.5 * AOV
```
The result:
```
ObjectDistance = CameraHeight / Sin( 180 - A - B = 90 + RoadPitch + y * AOV - CameraPitch - 0.5 * AOV ) * Sin( CameraPitch + 0.5 * AOV - y * AOV )
```

#### Pre-defined roadmap v.s. Run-time trigonometry computation in GPU

We may perform the geometry calculation at compile time, then store the result into a lookup table for later use. It is also possible to perform the geometry on-the-fly.

For this comparison, we use an embedded system GPU, the [VideoCore IV GPU architecture which is used on Raspberry pi 3](https://docs.broadcom.com/doc/12358545) as our reference. This comparison is theory-based. Since GPU, GPU API and OpenGL are highly abstract, the comparison may not be practical. Different GPU, different driver, different driver version, different optimization, different environment may produce different results.

__Pros of pre-defined roadmap__

- The road coordinates will never change; therefore, it is favored to calculate the coordinates once at the compile-time, then save them for later use. 

- Avoid run-time computation-intense operation such as trigonometry, which should saves GPU time (user time) and increases performance. 

- Avoid using large amount of intermediate value generated by run-time computation. Larger amount of intermediate value may excess the cache capacity and cause expensive cache miss.

- Avoid lose of precision in GPU. GPU uses lower precision numbers than CPU. Computation intensive operation suffers from greater precision lose as [_"Typically, the precision of operations must preserve the storage precisions of the variables involved. The only exceptions allowed are for a small number of computationally intensive built-in functions, e.g. atan(), which may return results at less than the declared precisions."_](https://www.khronos.org/files/opengles_shading_language.pdf#page=38)

- Support complex road conditions where the road-domain coordinates cannot be simply described by trigonometry. 

__Cons of pre-defined roadmap__

- Uses more texture buffer, higher memory bandwidth requirements. 

- Intense use of texture and memory lookup unit (TMU). For RPI, there is only one or two FIFO buffered TMU per GPU slice. Intense accessing TMP stalls the slice. 

Note:
- This comparison uses VideoCore IV which is seen in RPI3 as reference. Raspberry pi 4 is more mainstream than PRI3, which uses VideoCore VI. However, there is no document available for this VideoCore VI currently. We believe that VideoCore VI may share some similarity with its predecessor videoCore IV, so we use the document of VideoCore IV for reference. However, it is also worth pointing out that VideoCore VI supports OpenGL ES3.1; but its predecessor videoCore IV only supports OpenGL ES 2.x. Therefore, there may be some fundamental difference between them. 

- According to the document, the VideoCore IV GPU may come with 1 or 2 TMUs. GPUs with multiple TMUs are favored if the usage of texture lookup is high. 

### Orthographic projection

In most cases, a camera is placed on a pole near the road, facing toward the direction of traffic. the video is always in perspective view. 

However, in perspective view, further object is smaller than closer object, and all projection lines vanished in the center point of the view plane. In another word, an object travel along the forward-backward direction in road-domain will not travel in the vertical direction in the screen-domain if it is not on the middle axis of the screen. This means, when the program is tracing an object, it cannot simply trace vertically. There will be a slope when tracing the object. This slope varies as the distance of the object from the middle axis. 

![Orthographic projection](docs/poProject.jpeg "Orthographic projection")

This image illustrates this problem. Objects that move alone lane B in the road-domain will be moving vertical in the screen-domain; but object move alone lane D in road-domain will be moving in a sloped direction in the screen-domain. 

One way is to add consideration of the slope when tracing the object. The slope can be calculated at compile time, during program initialization, or on-the-fly by the GPU every time a tracing is performed.

![Orthographic projected](docs/poProjected.jpeg "Orthographic projected")

Another way is to project the image into an orthographic view. In this orthographic view, further pixels are stretched horizontally, so their width will be the same as closer pixels. By doing this, objects travel along the forward-backward direction in road-domain will now travel in the vertical direction in the screen-domain no matter how far it is from the middle axis. After the projection, the program can simply trace in the vertical direction without worrying about the slope. 

There is a lookup table in the roadmap that the program can use during projection. Projection is also possible in mathematical way.

![Vertices project](docs/verticeProject.png "Vertices project")

A __naive__ idea is to encode the transform into vertices array and take the advantage of GPU interpolation in fragment shader. In this ideal case, (B) shows the result of shifting top-right vertices left from (A). In this case, we can think (B) is the perspective view and (A) is the orthographic view. In fact, GPU draw meshes using triangles instead of rectangle, refer to (C). By moving the top-right vertices, it only transforms the top-right portion of the image (D); if we move both top vertices, the result is total distortion (E).

Another idea is to use the reverse of projection matrix, a 4 * 4 matrix. This method is commonly used in 3D rendering, such as video games and EDA tools. This method also comes with downside, it requires multiply of two 4 * 4 matrix, which involves heavy mathematical computation.

The program should measure the speed of object in the orthographic view; however, when displaying the result, it should be transformed back to perspective view. So, the speed result can be aligned with object in input video. Therefore, an orthographic-to-perspective transformation lookup table is required as well.

### Search distance

When we perform speed measurement of an object, we need to know the location of that object at the beginning and ending of the duration, which are location (rx1, ry1) and (rx2, ry2). In another word, to find the speed of an active pixel at position (sx1, sy1) at frame N, we need to know that pixel's position (sx2, sy2) at frame N+1. The location (rx2, ry2) should be close to (rx1, ry1); the position (sx1, sy1) should be close to (sx2, sy2). The greater the speed, the larger the distance. 

In most cases, the program should be able to find the position (sx2,sy2) near the position (sx1, sy1). However, there are some exceptions, that there may only be (sx1, sy1) or (sx2, sy2). 

- At the moment of the object entering or exiting the focus region. 

- The program fails to detect the object in one frame, but success in another frame. 

- Noise.

In those cases, the program is only able to find (sx1,sy1) or (sx2,sy2) but not both; hence, the program cannot determine the speed of that object. In this situation, the program may end up with searching the entire video frame. The consequences are wasting computation power, finding a wrong coordinate, and producing wrong result. 

To compensate for this problem, we need to define a threshold of speed. This value defines the maximum possible distance an object could possibly travel during one video frame. The program should stop searching when it cannot find an object on this threshold. 

The threshold is a road-domain value; however, a computer is working in screen-domain. We will need to convert the threshold into search distance. The search distance is used to describe the maximum pixel distance an active pixel may travel during one video frame. 

In most cases, a camera is placed on a pole near the road, facing toward the direction of traffic. That means objects should move only in the vertical direction in the screen, with negligible horizontal movement. Therefore, when performing the search, we should only search in an up and down direction. In the roadmap, we should only define the up and down limit. 

#### Search distance v.s. limit

There are two ways to define the search distance. One way is to define the maximum screen-domain **displacement** that an object could travel, like this code:

```GLSL
int limit = 10;
ivec2 base = ivec2(800, 600);
for (ivec2 offset = ivec2(0,0); offset.y < limit; offset.y++) {
	texelFetchOffset(sampler, base, 0, offset);
}
```

Another way is to define the edge of search limit in screen-domain, like this code:

```GLSL
int limit = 10;
ivec2 base = ivec2(800, 600);
for (ivec2 idx = base; idx.y < base.y + limit; idx.y++) {
	texelFetch(sampler, idx, 0);
}
```

Both shaders compiled on Linux + GLFW + GLEW with NVidia GPU driver; however, on Raspberry Pi, the first one (offset displacement method) cannot be compiled on Linux + GLFW + GLEW with RPI driver. Although this behavior is not documented in the [OpenGL ES 3.1 document](https://www.khronos.org/registry/OpenGL-Refpages/es3/html/texelFetchOffset.xhtml), it is suspected some OpenGL driver requires ```const``` value for the ```texelFetchOffset``` function argument. 

Although there is a workaround for the first method:

```GLSL
int limit = 10;
ivec2 base = ivec2(800, 600);
for (ivec2 offset = ivec2(0,0); offset.y < limit; offset.y++) {
	texelFetchOffset(sampler, base + offset, 0);
}
```

This will introduce an extra addition in each iteration. In each iteration, the second method performs one checking on ```idx``` and one increment on ```idx```; the workaround perform one checking on ```offset```, one increment on ```offset``` and one addition on ```base + offset```. Some compiler can optimize this code, some may not.

#### The actual search distance should be determined by the program

The search distance is the maximum distance that an object could reasonably travel between two frames. Since the input video FPS is not known, the distance cannot be calculated. Therefore, it does not make sense defining the limit in the roadmap.

However, what we know now is the top and bottom boundaries of the focus region. Because the program should only process data in the focus region; therefore, it is clear to say that there is no object outside of the focus region; hence, it is not necessary to continue the search beyond the boundaries of focus region. So, in the roadmap, we will assume the object could travel at infinite speed and use the focus region as search limit. The following strategy is used when generating the roadmap:

- If a pixel is inside the focus region, we will use the upper and lower edge of the focus region for the search limit. 

- If a pixel is outside the focus region, this pixel should be ignored by the fragment shader. We will use the y-coord of this pixel for the upper and lower search limits. So, even if this pixel gets processed, the fragment will not perform any search on this pixel. 

## File format

The roadmap is a binary file. This file contains 5 segments of data.

### Segment 1: Header

The first segment of the roadmap file is the header. The following C code describe the header.

```C
typedef struct FileHeader {
	uint32_t width, height;
	uint32_t pCnt;
	uint32_t fileSize;
} header_t;
```

The header contains two 32-bit unsigned integers ```width``` and ```height```, describing the dimension of the video frame and the size of this roadmap file. The header also includes a 32-bit unsigned integer ```pCnt```, which is used to indicate the number of road points.

There is another 32-bit unsigned integer ```fileSize```, used to describe the length of this file, exclude the optional meta data segment. This piece of data not only align the header into 128-bit chunk; but also can be used to check against ```width```, ```height``` and ```pCnt```. When the endian of the roadmap generator differs from the used of roadmap, they will not match.

This segment takes __4 * 4 = 16 bytes__.

### Segment 2: Table 1 - Road-domain geographic data

The second segment of the roadmap file is a table. This table is used to describe the road-domain geographic location associated with each pixel in both perspective view and orthographic view.

This table has ```height``` * ```width``` entries, where ```height``` and ```width``` are the same of those in the header segment. Each entry contains 4 32-bit floating-point numbers. Consider the following C code:

```C
typedef struct FileDataTable1 {
	float px, py;
	float ox;
	float pw;
} data1_t;

int height = 1080, width = 1920;
data1_t data1[height][width];
```

For examples, if there is an active pixel on screen position (x=37px, y=29px) in the perspective view; that means, there is an object on road location (x=```data1[29][37].px```, y=```data1[29][37].py```).

For orthographic view, because the y-coord of orthographic view is the same as y-coord of perspective view, adding a ```oy``` seems redundant. To get the location of any object in orthographic view, use ```(data1[y][x].ox, data1[y][x].px)```.

This table also include a member variable ```pw```, which describes the width of a pixel in perepective view. This value is the difference between two neighbor pixel's ```px``` (```table[y][x+1].px - table[y][x].px```), it can be used to calculate the size of object in the perespective view. For example, if an object at (x=37px, y=29px) is five-pixel wide, it means the width of that object is ```table[29][37].pw * 5```. It is worth to mention that the height of any object is proportional to```pw```; but the difference between ```table[y+1][x].py``` and ```table[y][x].py``` is on the far-close axis of the road, which is not proportional to object's height. The value ```pw``` can be calculated on-the-fly oin the shader by subtracting the ```px``` value of two neighbor pixel; however, this requires an additional pixel fetch and suffers erro from low-percision arithmetic hardware of GPU.

This segment takes __```height``` * ```width``` * 4 * 4 bytes__.

### Segment 3: Table 2 - Search distance & Conversion lookup

The third segment of the roadmap file is another table. This table contains search distance and perspective-orthographic conversion lookup of each pixel.

This table has ```height``` * ```width``` entries, where ```height``` and ```width``` are the same of those in the header segment. Each entry contains 4 32-bit floating-point numbers. Consider the following C code:

```C
typedef struct FileDataTable2 {
	float searchLimitUp, searchLimitDown;
	float lookupXp2o, lookupXo2p;
} data2_t;

int height = 1080, width = 1920;
data2_t data2[height][width];
```

The first two elements ```searchLimitUp``` and ```searchLimitDown``` donates the upward and downward search limits. The search limits are intended to be used in orthographic view. For example, if there is an active pixel at screen-domain position (x,y), the program should search upward until it reaches (x,```table2[y][x].searchLimitUp```) and downward until it reached (x,```table2[y][x].searchLimitDown```).

The last two elements ```lookupXp2o``` and ```lookupXo2p``` are used to transform the video between orthographic view and the perspective view. For example, if a pixel at located at (x,y) in perspective view, it should be project to (```table2[y][x].lookupXp2o```,y) in orthographic view. The transfomation is applied to horizontal direction only.

Coordinates in this table are all NTC, where the top-left corner is (0.0, 0.0) and the bottom-right corner is (1.0, 1.0).

Data in this table are all relative to pixels on screen. It would makes more sense usinge integer in this table. However, integer texture using ```isampler``` does not work on Raspberry Pi 4. Although RPI4 should support GLES 3.1, and ```isampler``` was standardized since [GLES 3.0](https://www.khronos.org/files/opengles3-quick-reference-card.pdf).

There is advantage of using floating-point number to represent pixels. When the size of roadmap and the size of video are different, absolute coordinates need to be convert before reading. For example, if the roadmap is 1920-px wide by 1080-px high and the video is 1280-px wide and 720px-high. To find the perspective-to-orthographic projection on pixel(x=50px, y=30px), we will need to do:

```GLSL
vec2 pos = vec2(x, y); //Provided by hardware in NTC: pos = vec2(50f/1280f, 30f/720f) = vec2(0.03906,0.04167)
ivec2 mapIdx = ivec2( pos * vec2(textureSize(map, 0)) ); //textureSize(map, 0) returns the size of map: ivec2(1920, 1080)
ivec2 videoIdx = ivec2( pos * vec2(textureSize(video, 0)) ); //textureSize(video, 0) = ivec2(1280, 720)

int proj = texelFetch(map, mapIdx, 0).z; //lookupXp2o
ivec2 projIdx = ivec2(proj, videoIdx.y);

vec4 color = texelFetch(video, projIdx, 0); //Output
```

As the example shows, using absolute coordinate involves a lot of coordinates computation. These computation not only consumes GPU time, but also generates error when rounding or normalizing the floating-point numbers during computation. On the other hand, using NTC could avoid the coordinate conversion between resources with different size, because the textures with different sizes are all normalized to the same range of size, which is the NTC ```[0.0, 1.0]```. To achieve the same functionality, we can do:

```GLSL
vec2 pos = vec2(x, y); //Provided by hardware in NTC: pos = vec2(50f/1280f, 30f/720f) = vec2(0.03906,0.04167)
float proj = texture(map, pos).z;
vec2 projPos = vec2(proj, pos.y);
vec4 color = texture(video, projPos);
```

In this example, using NTC instead of absolute coordinate reducing the amount of computation in the shader. The first example uses ```texelFetch()``` to fetch a specific pixel from the map, using absolute coordinate as index; the second example uses ```texture()``` to sample a specific point from the map, using NTC. The Texture and Memory lookup Unit, or TMU, in the GPU hardware takes will convect NTC to the actual address in the memory.

Other than coordinate, there is another difference between ```texture()``` and ```texelFetch()```. ```texelFetch()``` will directly fetch the specific pixel from texture; however, ```texture()``` will perform sampling. If the texture filter paramter is ```GL_*LINEAR```, the TMU will perform sampling on data by calculating the weighted average of nearest four tiles (linear filter). That means, the result of ```texture()``` has been processed.

It does not pose any hazard for sampling roadmap, all data can be interpolated. For example, road information associated to screen(0.32, 0.58) should be close to the aveaage of that of screen(0.31, 0.57) and screen(0.33, 0.59), because road is a continuous surface. Howeer, it may not be safe to interpolate some data. For example, a texture contains edge should not be interpolated, because the texture is not continuous in the idrection perpendicular to the edge. Instead, the following code should be used:

```GLSL
vec2 pos = vec2(x, y);
float proj = texture(map, pos).z;
vec2 dataPos = vec2(proj, pos.y);
ivec2 dataIdx = ivec2( dataPos * vec2(textureSize(src, 0)) );
result = texelFetch(data, dataIdx, 0);
```

Using floating-point number impose restriction on the frame size. In embeded system, some system may not support full size 32-bit floating-point number (IEEE-754 32bit FP). For [GLES 2.0 compatible devices](https://www.khronos.org/files/opengles_shading_language.pdf#page=39), a ```mediump``` (16-bit) unsigned integer can be used to perciselly represent any integer between 0 to 2^16-1, or 65535. However, a ```mediump``` floating-point number only has 10 bits of fraction, hence lacks level of precision. _"It is required that the fragment language provide floating point variables whose range is at least (-16384, +16384) and whose precision is at least one part in 1024"_, which means, some devices may only support accurate pixel fetch of texure up to 1024px when using sampler with NTC.

Since the mainstream video devices today support 720p or 1080p video, we will double the restriction to 2048px. This may produce up to 1px error, but acceptable.

Nowadays, most mainstream GPUs begin to support full size IEEE-754 32-bit floating-point number, or even 64-bit double precision floating-point number. However, using half the data width brings double the speed. This is because for SIMD processors, multiple sets of shorter-length data can be fit into the processor at the same time. Especially for embedded devide which lacks the power of desktop GPU, using ```mediump``` could dramatically boost the performance compare to using ```highp```.

This segment takes __```height``` * ```width``` * 4 * 4 bytes__.

### Segment 4: Focus region

The fourth segment of the roadmap file is a list of points. Consider the following C code:

```C
typedef struct Point_t {
	float rx, ry;
	float sx, sy;
} point_t;

unsigned int pCnt = 10 * 2 + 4;
point_t points[pCnt];
```

Each point represents a road-domain point, where its location is given by ```rx``` and ```ry```. Each point is associate to a screen-domain pixel, where its position is described by ```sx``` and ```sy```. 

Road points are used to describe the screen-domain focus region. It encircles an area of pixels that should be processed by the program, the "focus region". Pixels outside of this area should be ignored. This method not only saves computation power by reducing the amount of pixel that requires processing, but also avoids certain areas that should be ignored. 

In most cases, road points are used to define the boundary of the road. Because of this, road points come in pairs. ```points[ i * 2 + 0 ]``` is always the left point, ```points[ i * 2 + 1 ]``` is always the right point. Therefore, ```points[ i * 2 + 0 ].rx``` and ```points[ i * 2 + 0 ].sx``` is always less than ```points[ i * 2 + 1 ].rx``` and ```points[ i * 2 + 1 ].sx``` respectively. 

In the road points list, the order is from further to closer in road-domain (from top to bottom in screen-domain). Therefore, ```points[ i * 2 + 0 ].screenY``` is always less than ```points[ i * 2 + 2 ].screenY```; but ```points[ i * 2 + 0 ].roadY``` is always greater than ```points[ i * 2 + 2 ].roadY```.

Screen coordinates ```sx``` and ```sy``` are in NTC.

There are N road points. The first N - 4 points are used for focus region of the perepective view, the last 4 points are used for orthographic view.

This segment takes __```pCnt``` * 4 * 4 bytes__.

## Segment 5: Meta data

The last segment stores human-readable data in ASCII format, such as comments and notes. The content of this segment is arbitrary, and the length of this segment is arbitrary. This section should be ignored by the program. 

## Size chart
- Seg 1: 16 bytes
- Seg 2: ```height``` * ```width``` * 4 * 4 bytes
- Seg 3: ```height``` * ```width``` * 4 * 4 bytes
- Seg 4: 4 + 2 * ```pCnt``` * 4 * 4 bytes
- Seg 5: DNC
- Total: (```height``` * ```width``` * 2 + ```pCnt``` + 1) * 16 bytes + DNC