## bmp2array4bit

bmp2array4bit.py reads a bmp file, and creates C (or C++) code that contains two arrays for adding images to four-bit sprites.  See [Sprite_image_4bit](../../examples/Sprite/Sprite_image_4bit) for an example.

It is loosely based on Spark Fun's bmp2array script, https://github.com/sparkfun/BMPtoArray/blob/master/bmp2array.py.  The bmp file format is documented in https://en.wikipedia.org/wiki/BMP_file_format.

You'll need python 3.6 (the original uses Python 2.7)

`usage: python bmp2array4bit.py [-v] star.bmp [-o myfile.c]`

Create the bmp file in Gimp (www.gimp.org) from any image as follows:

* Remove the alpha channel (if it has one)
        Layer -> Transparency -> Remove Alpha Channel
* Set the mode to indexed.
        Image -> Mode -> Indexed...
* Select Generate optimum palette with 16 colors (max)
* Export the file with a .bmp extension. Do **NOT** select options:
  * Run-Length Encoded
  * Compatibility Options: "Do not write color space information" 
  * There are no Advanced Options available with these settings

(There are other tools that will produce bmp files, and these should work provided you don't use run-length encoding or other advanced features).

The first array produced is the palette for the image.
The second is the image itself.
