'''
Video To BitmapVideo Convertor 
This simple mini program is used to convert common video file to readable plain bitmaps. 


THIS PROGRAM IS NOT THE CORE PART OF THIS PROJECT! THIS PROGRAM IS USED TO PREPARE TEST DATA FOR THIS PROJECT. 
A 1-minute-long 1080p 30FPS RGB24 video contains about 10GB of data. 
Common video files (like .mp4, .avi, .mov) usually compress the video data to minimize storage space and memory bandwidth 
requirements. Compression and decompression (codec) algorithm are complex, and different format may 
use different algorithms. The codec is not what we are studying here. We do not want to deal with codec or 
compressed video in this project; instead, we will only deal with easy-to-read plain bitmaps. 
By "plain", it means:
- A file contains multiple frames. Frame 1 first, then frame 2, then frame 3...
- A frame has width * height pixels. The order is left-to-right, then top-to-bottom, like how we write in most languages (e.g. English, morden Chinese). 
- A pixel has 1, 3 or 4 bytes, which is mono, RGB or RGBA. 
So, a 1 minute long 1080p 30FPS RGB24 video is about 60MB in .mov format, but with RGB scheme there are 
60s * 30fps * (1920*1080)px/frame * 3byte/px = 11,197,440,000 bytes in plain bitmap video format. 
That's a lot of data need to put on the disk or in our memory. Since we are now developing the algorithm on a high-end 
platform, this is not a big issue. But, when we implement this program in our product, we need to switch to 
streaming mode (so there is only few frames in memory buffer). 
'''


import sys
import cv2
import numpy

src =		'on3.mov'		#The input file, a video file in common video format such as .mp4
dest =		'../../on3-720v10.data'	#Where to save the plain bitmap video 
width =	1280			#Output frame size in pixel
height =	720
fpsSkip =	2			#Skip frames. E.g.: if src is 30fps and skip is 2, dest will be 30 / (1+2) = 10fps
scheme =	cv2.COLOR_BGR2RGBA	#The default scheme of openCV is BGR, the output can be RGBA, RGB or GRAY

'''
Most video contains RGB 3 channels. 
RGBA consumes extra 25% space, but align pixels in 32-bit words, which makes GPU uploading faster. 
Gray/Mono consumes least amount of memory, but lacks color. 
'''

count = 0
outputFile = open(dest, 'wb')
video = cv2.VideoCapture(src)
if video.isOpened():
	meta = (video.get(cv2.CAP_PROP_FRAME_WIDTH), video.get(cv2.CAP_PROP_FRAME_HEIGHT), video.get(cv2.CAP_PROP_FPS))
	print('Src video size: {}px * {}px @ {}fps'.format(*meta))
	print('Dest video size: {}px * {}px @ {}fps'.format(width, height, meta[2]/(1+fpsSkip)))
while True:
	ret, frame = video.read()
	if frame is None or count >= 2400:
		print('Done, end of video reached or limit reached, {} frames processed.'.format(video.get(cv2.CAP_PROP_FRAME_COUNT)))
		break
	if count % (fpsSkip + 1) == 0:
		frame = cv2.cvtColor(frame, scheme)
		frame = cv2.resize(frame, (width, height))
		outputFile.write(bytearray(frame))
	count = count + 1
outputFile.close()