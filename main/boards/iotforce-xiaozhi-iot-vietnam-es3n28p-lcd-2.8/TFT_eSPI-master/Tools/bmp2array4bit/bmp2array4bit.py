'''

    This script takes in a bitmap and outputs a text file that is a
    byte array used in Arduino files.

    It is loosely based on Spark Fun's bmp2array script.

    You'll need python 3.6 (the original use Python 2.7)

    usage: python fourbitbmp2array.py [-v] star.bmp [-o myfile.c]
    
    Create the bmp file in Gimp by :

    . Remove the alpha channel (if it has one) Layer -> Transparency -> Remove Alpha Channel
    . Set the mode to indexed.  Image -> Mode -> Indexed...
    . Select Generate optimum palette with 16 colors (max)
    . Export the file with a .bmp extension. Options are:
        . Run-Length Encoded: not selected
        . Compatibility Options: "Do not write color space information" not selected
        . There are no Advanced Options available with these settings


    

'''

import sys
import struct
import math
import argparse
import os

debug = None

def debugOut(s):
    if debug:
        print(s)

# look at arguments
parser = argparse.ArgumentParser(description="Convert bmp file to C array")
parser.add_argument("-v", "--verbose", help="debug output", action="store_true")
parser.add_argument("input", help="input file name")
parser.add_argument("-o", "--output", help="output file name")
args = parser.parse_args()

if not os.path.exists(args.input):
    parser.print_help()
    print("The input file {} does not exist".format(args.input))
    sys.exit(1)

if args.output == None:
    output = os.path.basename(args.input).replace(".bmp", ".c")
else:
    output = args.output

debug = args.verbose

try:
    #Open our input file which is defined by the first commandline argument
    #then dump it into a list of bytes
    infile = open(args.input,"rb") #b is for binary
    contents = bytearray(infile.read())
    infile.close()
except:
    print("could not read input file {}".format(args.input))
    sys.exit(1)

# first two bytes should be "BM"
upto = 2
#Get the size of this image
data = struct.pack("BBBB", contents[upto], contents[upto+1], contents[upto+2], contents[upto+3])
fileSize = struct.unpack("I", bytearray(data))

upto += 4
# four bytes are reserved

upto += 4

debugOut("Size of file: {}".format(fileSize[0]))

#Get the header offset amount
data = struct.pack("BBBB", contents[upto], contents[upto+1], contents[upto+2], contents[upto+3])
offset = struct.unpack("I", bytearray(data))

debugOut("Offset: {}".format(offset[0]))
upto += 4

data = struct.pack("BBBB", contents[upto], contents[upto+1], contents[upto+2], contents[upto+3])
headersize = struct.unpack("I", bytearray(data))
headerLength = headersize[0]
startOfDefinitions = headerLength + upto
debugOut("header size: {}, up to {}, startOfDefinitions {}".format(headersize[0], upto, startOfDefinitions))
upto += 4

data = struct.pack("BBBB", contents[upto], contents[upto+1], contents[upto+2], contents[upto+3])
t = struct.unpack("I", bytearray(data))
debugOut("width: {}".format(t[0]))
width = t[0]

upto += 4
data = struct.pack("BBBB", contents[upto], contents[upto+1], contents[upto+2], contents[upto+3])
t = struct.unpack("I", bytearray(data))
debugOut("height: {}".format(t[0]))
height = t[0]

# 26
upto += 4

data = struct.pack("BB", contents[upto], contents[upto+1])
t = struct.unpack("H", bytearray(data))
debugOut("planes: {}".format(t[0]))

upto = upto + 2
data = struct.pack("BB", contents[upto], contents[upto+1])
t = struct.unpack("H", bytearray(data))
debugOut("bits per pixel: {}".format(t[0]))
bitsPerPixel = t[0]

upto = upto + 2
data = struct.pack("BBBB", contents[upto], contents[upto+1], contents[upto+2], contents[upto+3])
t = struct.unpack("I", bytearray(data))
debugOut("biCompression: {}".format(t[0]))

upto = upto + 4
data = struct.pack("BBBB", contents[upto], contents[upto+1], contents[upto+2], contents[upto+3])
t = struct.unpack("I", bytearray(data))
debugOut("biSizeImage: {}".format(t[0]))

upto = upto + 4
data = struct.pack("BBBB", contents[upto], contents[upto+1], contents[upto+2], contents[upto+3])
t = struct.unpack("I", bytearray(data))
debugOut("biXPelsPerMeter: {}".format(t[0]))

upto = upto + 4
data = struct.pack("BBBB", contents[upto], contents[upto+1], contents[upto+2], contents[upto+3])
t = struct.unpack("I", bytearray(data))
debugOut("biYPelsPerMeter: {}".format(t[0]))

upto = upto + 4
data = struct.pack("BBBB", contents[upto], contents[upto+1], contents[upto+2], contents[upto+3])
t = struct.unpack("I", bytearray(data))
debugOut("biClrUsed: {}".format(t[0]))
colorsUsed = t

upto = upto + 4
data = struct.pack("BBBB", contents[upto], contents[upto+1], contents[upto+2], contents[upto+3])
t = struct.unpack("I", bytearray(data))
debugOut("biClrImportant: {}".format(t[0]))

upto += 4

debugOut("Upto: {} Number of colors used: {} definitions start at: {}".format(upto, colorsUsed[0],  startOfDefinitions))

#Create color definition array and init the array of color values
colorIndex = [] #(colorsUsed[0])
for i in range(colorsUsed[0]):
    colorIndex.append(0)

#Assign the colors to the array.  upto = 54
# startOfDefinitions = upto
for i in range(colorsUsed[0]):
    upto =  startOfDefinitions + (i * 4)
    blue = contents[upto]
    green = contents[upto + 1]
    red = contents[upto + 2]
    # ignore the alpha channel.

    # data = struct.pack("BBBB", contents[upto], contents[upto+1], contents[upto+2], contents[upto+3])
    # t = struct.unpack("I", bytearray(data))
    # colorIndex[i] = t[0]

    colorIndex[i] = (((red & 0xf8)<<8) + ((green & 0xfc)<<3)+(blue>>3))
    debugOut("color at index {0} is {1:04x}, (r,g,b,a) = ({2:02x}, {3:02x}, {4:02x}, {5:02x})".format(i,  colorIndex[i], red, green, blue, contents[upto+3]))

#debugOut(the color definitions
# for i in range(colorsUsed[0]):    
#     print hex(colorIndex[i])

# perfect, except upside down.

#Make a string to hold the output of our script
arraySize = (len(contents) - offset[0]) 
outputString = "/* This was generated using a script based on the SparkFun BMPtoArray python script" + '\n'
outputString += " See https://github.com/sparkfun/BMPtoArray for more info */" + '\n\n'
outputString += "static const uint16_t palette[" + str(colorsUsed[0]) + "] = {";
for i in range(colorsUsed[0]): 
    # print hexlify(colorIndex[i])
    if i % 4 == 0:
        outputString += "\n\t"
    outputString += "0x{:04x}, ".format(colorIndex[i])

outputString = outputString[:-2]
outputString += "\n};\n\n"
outputString += "// width is " + str(width) + ", height is " + str(height) + "\n"
outputString += "static const uint8_t myGraphic[" + str(arraySize) + "] PROGMEM = {" + '\n'

if bitsPerPixel != 4:
    print("Expected 4 bits per pixel; found {}".format(bitsPerPixel))
    sys.exit(1)
    
#Start converting spots to values
#Start at the offset and go to the end of the file
dropLastNumber = True #(width % 4) == 2 or (width % 4) == 1
paddedWidth = int(math.ceil(bitsPerPixel * width / 32.0) * 4)
debugOut("array range is {} {} len(contents) is {} paddedWidth is {} width is {}".format(offset[0], fileSize[0], len(contents), paddedWidth, width))

r = 0
width = int(width / 2)
#for i in range(offset[0], fileSize[0]):                 # close but image is upside down.  Each row is correct but need to swap columns.
#for i in range(fileSize[0], offset[0], -1):

for col in range(height-1, -1, -1):
    i = 0
    for row in range(width):
        colorCode1 = contents[row + col*paddedWidth + offset[0]]  

        if r > 0 and r % width == 0:
            i = 0
            outputString += '\n\n'
        elif (i + 1) % 12 == 0 :
            outputString += '\n'
            i = 0
        
        #debugOut("cell ({0}, {1})".format(row, col)

        r = r + 1
        i = i + 1
        outputString += "0x{:02x}, ".format(colorCode1)


    
#Once we've reached the end of our input string, pull the last two
#characters off (the last comma and space) since we don't need
#them. Top it off with a closing bracket and a semicolon.
outputString = outputString[:-2]
outputString += "};"

try:
    #Write the output string to our output file
    outfile = open(output, "w")
    outfile.write(outputString)
    outfile.close()
except:
    print("could not write output to file {}".format(output))
    sys.exit(1)

debugOut("{} complete".format(output))
debugOut("Copy and paste this array into a image.h or other header file")

if not debug:
    print("Completed; the output is in {}".format(output))
