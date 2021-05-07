# SpeedCamera

```
----------       ------------------       ---       ------------       -----------       ---       -------------
| Camera |  -->  | Edge Detection |  -->  |X|  -->  |Projection|  -->  | Compare |  -->  |X|  -->  | Speed Map |
----------       ------------------       ---       ------------       -----------       ---       -------------
                                     Accelerometer                                  Camera Position
```
- Camera:                     Record video, send video frame to next stage.
- Edge Detection:             Find the edge of object.
- Acceletometer & Projection: Compensate shifting of camera, so the image is always stable.
- Compare:                    Find the displacement of object edge between two frame.
- Camera Position:            Using camera position and pixel position to find object distance, then use displacement of object to find speed.
- Speed map:                  Speed of object saved in 2D map.

## How it work

For computer, it is quite hard to directly reconize something is moving in a video. What computer see is not cars on road, but tons of pixels with different color.

Someone may consider using AI. Finding a model, trainning the model, and the magic just happen. However, at current stage, AI is not quite a reliable technolodge. Statical may show the AI model is capable of giving the result with some percent of correction rate. But, internally, AI is just some matrix multiplcation, people will say "The trainning model gives all the weight", but people cannot say the actual reason of chosing that value. Also, there is not enough confident for people to leave life-reliable task to AI, such as auto-driving car, at least at current stage.

Furthermore, AI computation will take a lot of computation power. There are too much matrix calculation.

Compare to the AI solution, this project is more on old-school favor.

When we say an object is moving, the boundary (or in another word, the edge) is moving as well. For computer, finding the edge is relativelty simple, just apply a kernel filter. Generally speaking, this program dose a very simple job, measure the displacement of the dege in a giving time period, and by using Grade 4 science: ```speed = displacement / time``` Goal!

## Process flow

This section describe how the prototype work. For performance reason, and to make my life easy (reduce edge-case), the actual implementation may be slightly differ from the this algorithm described in this section. For detail implementation, refer to the code.

### Step 0: Fetch video from source device

There are multiple ways to provide video to this project. Depending on the type of camera, there are:
- USB camera, the camera driver writes the frame into a dedicated buffer.
- CCTV camera, an AV interface card decodes the analog signal and write the frame in a dedicated buffer.
- Web camera, a dedicated software downloads the video from internet.

For demonstration purpose, and to make our life eaiser, in this project, let's use a video file.

Common video file formats (such as mp4, avi, mov) are compressed to save storage space. A decoder is required to decompress the file and get the actual bitmap image of each frame of the video. Since there are lots of compress formats and some formats is not open-source, we will use the codec API comes with opencv to decompress the video for us.

So, step 0 of this process is to decompress the video into plain bitmaps. This is done by using the script ```/source/video2bmpv.py```

This script generates easy-to-read plain bitmaps. Comsider the following C code to understant the so called "easy-to-read plain bitmaps":
```C
struct pixel { /* A pixel contains 3 8-bit unsigned int, which are the R, G and B channels */
	uint8_t r, g, b;
};
struct frame { /* A frame conatins HEIGHT lines, each line contains WIDTH dots, each dot is a pixel */
	struct pixel dot [HEIGHT][WIDTH]
}
struct video { /* There are TIME (in seconds) times FPS (frame pre second) frames in one video file */
	struct frame f[TIME*FPS]
}
```

Now, we have the above script generated the required readable plain video file for us, we can turely start our process.

### Step 1: Load a frame

Now, let's move to this directory: ```/process/prototype```

We will load one frame at a time. Class ```./source.x``` is used to load the frame from the plain bitmap file. This calss is basiclly a wrapper of ```fopen(), fread() and fclose()```.

![Process::Raw](/docs/raw.jpg)

### Step 2: Edge detection

Now, apply a kernel filter to the frame using the edge class in ```./edge.x```. The input is a colorful image, after the filter, the output is a gray-scale image only contains edge of everything of the image.

![Process::Edge](/docs/edge.jpg)

### Step 3: Projection

The camera may be mounted on unstable platform. For example, when the wind is vary strong, the camera shifts.

The project class in ```./project.x``` is used to compensate the shift of camera. So, the video window (clip-space) may be moving, but the projected image is always stable.

TODO: Use software leveling or hardware gyro sensor.

The following screenshot assume the camara has been shift left by 10 degree.

![Process::Project](/docs/project.jpg)

### Step 4: Compare the displacement of edge (on screen)

This compare class in ```./compare.x``` is used to compare the displacement of edges between current frame and previous frame.

Assume the edge was at screen position A (SAx, SAy), and is now at screen position B (SBx, SBy). So, speed is ```[ (SBx - SAx) ^ 2 + (SBy - SAy) ^ 2 ] ^ 0.5```

Consider the following image, the strength of luma represents the speed of object. we know closer objects are larger, and the displacements in pixel are also greater than further objects. So, the luma strength of closer objects' edge is greater.

![Process::ScreenSpeed](/docs/screenspeed.jpg)

### Step 5: Pixel displacement to real world speed

We have the displacement of object on the screen in unit of pixel. When we install the camera, we know the each pixel of the camera represents a actual coordnate in real world. It is quite easy for us to convert the coordnates on screen to coordnates in real world.

Assume the edge was at screen position A (SAx, SAy), which represents real world position location A (WAx, WAy, WAz); and is now at screen position B (SBx, SBy), which represents real world position location B (WBx, WBy, WBz). So, speed is ```[ (WBx - WAx) ^ 2 + (WBy - WAy) ^ 2 + (WBz - WAz) ^ 2 ] ^ 0.5```