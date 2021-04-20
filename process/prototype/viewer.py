programInfo = '''
BitmapVideo to Video Convertor (mp4)
Command: python ./this.py sourceFileName width height fps colorScheme [outputFileName], where:
- sourceFileName: The input file, a plain bitmap video file
- width, height: Width and height of the video in unit of pixel
- colorScheme: 1 = mono gray, 2 = RGB565, 3 = RBG8, 4 = RGBA8
- fps: Frame pre second
- outputFileName: Write video to "$outputFileName.avi"
This simple mini program is used to convert readable plain bitmaps to mp4 video file.
'''
programDesc = '''
THIS PROGRAM IS NOT THE CORE PART OF THIS PROJECT! THIS PROGRAM IS USED TO VIEW INTERMIDIA DATA FOR THIS PROJECT.

'''

import sys
import cv2
import numpy

if len(sys.argv) < 6:
	print(programInfo)
	exit()

width = int(sys.argv[2])
height = int(sys.argv[3])
fps = int(sys.argv[4])
colorScheme = int(sys.argv[5])

colorSchemeDesc = 'Color scheme not supported'
if colorScheme == 1:
	colorSchemeDesc = 'Mono/Gray'
elif colorScheme == 3:
	colorSchemeDesc = 'RGB'
elif colorScheme == 4:
	colorSchemeDesc = 'RGBA'
else:
	print(colorSchemeDesc)
	exit()

outputFile = False
if len(sys.argv) >= 7:
	codec = cv2.VideoWriter_fourcc('M','J','P','G')
	outputFile = cv2.VideoWriter(sys.argv[6]+'.avi', codec, fps, (width, height))

print('Read bitmaps from {}, resolution {} * {} {}, FPS = {}'.format(sys.argv[1], sys.argv[2], sys.argv[3], colorSchemeDesc, sys.argv[4]))
if outputFile:
	print('Render result save in file: {}.avi'.format(sys.argv[6]))
else:
	print('Render result shown in pop-up window.')
	
framePeriod = int( 1000 / fps )

frame = numpy.zeros((height, width, 3), numpy.uint8)

count = 0
raw = open(sys.argv[1], 'rb')
while True:
	print('\rProcessing: '+str(count), end='')
	buffer = raw.read(width * height * colorScheme)
	if len(buffer) == 0:
		break
	y = 0
	while y < height:
		x = 0
		while x < width:
			if (colorScheme == 1):
				luma = buffer[y * width + x]
				frame[y][x] = [luma, luma, luma]
			else:
				r = buffer[ (y * width + x) * colorScheme ]
				g = buffer[ (y * width + x) * colorScheme + 1 ]
				b = buffer[ (y * width + x) * colorScheme + 2 ]
				frame[y][x] = [r, g, b]
			x = x + 1
		y = y + 1
	if outputFile:
		outputFile.write(frame)
	else:
		cv2.imshow('Debug Viewer', frame)
		cv2.setWindowTitle('Debug Viewer', 'Debug Viewer - frame '+str(count))
		if cv2.waitKey(framePeriod) & 0xFF == ord('q'):
			break
	count = count + 1

raw.close()
if outputFile:
	outputFile.release()
else:
	cv2.destroyAllWindows()

print('\rDone! {} frames processed'.format(count))