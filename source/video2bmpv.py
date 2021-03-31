programInfo = '''
Video To BitmapVideo Convertor
Command: python ./this.py sourceFileName destFileName [maxFrameCount], where:
- sourceFileName: The input file, a video file in common video format such as .mp4
- destFileName: Where to save the plain bitmap video
- maxFrameCount: Stop after this much frames (in case the input video is very long)
This simple mini program is used to convert common video file to readable plain bitmaps.
'''
programDesc = '''
THIS PROGRAM IS NOT THE CORE PART OF THIS PROJECT! THIS PROGRAM IS USED TO PREPARE DATA FOR THIS PROJECT.
A 1 minute long 1080p 30FPS RGB24 video contains about 10GB of data.
Common video files (like .mp4, .avi, .mov) usually compress the video data to minimize storage space
requirements. Compression and decompression (codec) algorithm are complex, and different format may
use different algorithms. The codec is not what we are studying here. We do not want to deal with codec or
compressed video in this project; instead, we will only deal with easy-to-read plain bitmaps.
By "plain", it means:
- A file contains multiple blocks, each block is a frame of the video. Frame 1 first, then frame 2, then frame 3...
- A frame has width * height pixels. The order is left-to-right, top-to-bottom, like how we write English.
- A pixel has 3 or 4 bytes, which is RGB(A).
So, a 1 minute long 1080p 30FPS RGB24 video is about 60MB in .mov format, but
60s * 30fps * (1920*1080)pixel/frame * 3byte/pixel = 11,197,440,000 bytes in our plain bitmap video format.
That's a lot of data need to put on the disk or in our memory. Since we are now developing the algorithm on a high-end
platform, this is not a big issue. But, when we implement this in our product, we need to switch to
streaming mode (so there is only few frames in memory buffer).
'''

import sys
import cv2

dataStructure = cv2.COLOR_BGR2RGB # COLOR_BGR2RGBA or COLOR_BGR2RGB
'''
RGBA consumes extra 25% space, but align pixels in 32-bit word.
Generally speaking, this is a trade-off between memory size and computation speed.
32-bit alignment is vector instruction sets friendly; but smaller data size has cache advantages.
This is a really complex trade off, and the result may differ on different CPU (arch), OS (cache miss
context switch strategy), compiler...
Don't ask me for why BGR, the opencv guy decided so.
'''

if len(sys.argv) < 3:
	print(programInfo)
	exit()
maxFrame = int(sys.argv[3]) if len(sys.argv) > 3 else 60 * 1200 #60FPS * 20mins is way more than enough
print('Read video from {}, save in {}, max frame = {}'.format(sys.argv[1], sys.argv[2], maxFrame))

count = 0
outputFile = open(sys.argv[2], 'wb')
video = cv2.VideoCapture(sys.argv[1])
while video.isOpened():
	ret, frame = video.read()
	if count == 0:
		fps = video.get(cv2.CAP_PROP_FPS)
		width = video.get(cv2.CAP_PROP_FRAME_WIDTH)
		height = video.get(cv2.CAP_PROP_FRAME_HEIGHT)
		print('Processing start. Resolution = {}*{}, {}FPS'.format(width, height, fps))
	if frame is None:
		print('Done, end of video reached, {} frames processed.'.format(video.get(cv2.CAP_PROP_FRAME_COUNT)))
		break
	colorFixed = cv2.cvtColor(frame, dataStructure)
	byteBlock = bytearray(colorFixed)
	outputFile.write(byteBlock)
	count = count + 1
	if count >= maxFrame:
		print('Done, {} frames processed.'.format(maxFrame))
		break
outputFile.close()