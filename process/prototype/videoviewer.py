programInfo = '''
BitmapVideo to Video Convertor (mp4)
Command: python ./this.py sourceFileName width height fps, where:
- sourceFileName: The input file, a plain bitmap video file
- width, height: Width and height of the video in unit of pixel
- fps: Frame pre second
This simple mini program is used to convert readable plain bitmaps to mp4 video file.
'''
programDesc = '''
THIS PROGRAM IS NOT THE CORE PART OF THIS PROJECT! THIS PROGRAM IS USED TO VIEW INTERMIDIA DATA FOR THIS PROJECT.

'''

import sys
import cv2
import numpy

if len(sys.argv) < 5:
	print(programInfo)
	exit()
print('Read bitmaps from {}, resolution {} * {},FPS = {}'.format(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]))
width = int(sys.argv[2])
height = int(sys.argv[3])
fps = int(sys.argv[4])

codec = cv2.VideoWriter_fourcc('M','J','P','G')
outputFile = cv2.VideoWriter('view.avi', codec, fps, (width, height))

frame = numpy.zeros((height, width, 3), numpy.uint8)

count = 0
raw = open(sys.argv[1], 'rb')
while True:
	print(count)
	buffer = raw.read(width * height)
	if len(buffer) == 0:
		break
	y = 0
	while y < height:
		x = 0
		while x < width:
			luma = buffer[y * width + x]
			frame[y][x] = [luma, luma, luma]
			x = x + 1
		y = y + 1
	outputFile.write(frame)
	count = count + 1

outputFile.release()
raw.close()
print('Done! {} frames processed'.format(count))