### A short guide to use fontconvert.c to create your own fonts using MinGW.

#### STEP 1: INSTALL MinGW

Install MinGW (Minimalist GNU for Windows) from [MinGW.org](http://www.mingw.org/).
Please read carefully the instructions found on [Getting started page](http://www.mingw.org/wiki/Getting_Started).
I suggest installing with the "Graphical User Interface Installer".
To complete your initial installation you should further install some "packages".
For our purpose you should only install the "Basic Setup" packages.
To do that:

1. Open the MinGW Installation Manager
2. From the left panel click "Basic Setup".
3. On the right panel choose "mingw32-base", "mingw-gcc-g++", "mingw-gcc-objc" and "msys-base"
and click "Mark for installation"
4. From the Menu click "Installation" and then "Apply changes". In the pop-up window select "Apply".


#### STEP 2: INSTALL Freetype Library

To read about the freetype project visit [freetype.org](https://www.freetype.org/).
To Download the latest version of freetype go to [download page](http://download.savannah.gnu.org/releases/freetype/)
and choose "freetype-2.7.tar.gz" file (or a newer version if available).
To avoid long cd commands later in the command prompt, I suggest you unzip the file in the C:\ directory.
(I also renamed the folder to "ft27")
Before you build the library it's good to read these articles:
* [Using MSYS with MinGW](http://www.mingw.org/wiki/MSYS)
* [Installation and Use of Supplementary Libraries with MinGW](http://www.mingw.org/wiki/LibraryPathHOWTO)
* [Include Path](http://www.mingw.org/wiki/IncludePathHOWTO)

Inside the unzipped folder there is another folder named "docs". Open it and read the INSTALL.UNIX (using notepad).
Pay attention to paragraph 3 (Build and Install the Library). So, let's begin the installation.
To give the appropriate commands we will use the MSYS command prompt (not cmd.exe of windows) which is UNIX like.
Follow the path C:\MinGW\msys\1.0 and double click "msys.bat". The command prompt environment appears.
Enter "ft27" directory using the cd commands:
```
cd /c
cd ft27
```

and then type one by one the commands:
```
./configure --prefix=/mingw
make
make install
```
Once you're finished, go inside "C:\MinGW\include" and there should be a new folder named "freetype2".
That, hopefully, means that you have installed the library correctly !!

#### STEP 3: Build fontconvert.c

Before proceeding I suggest you make a copy of Adafruit_GFX_library folder in C:\ directory.
Then, inside "fontconvert" folder open the "makefile" with an editor ( I used notepad++).
Change the commands so in the end the program looks like :
```
all: fontconvert

CC     = gcc
CFLAGS = -Wall -I c:/mingw/include/freetype2
LIBS   = -lfreetype

fontconvert: fontconvert.c
	$(CC) $(CFLAGS) $< $(LIBS) -o $@

clean:
	rm -f fontconvert
```
Go back in the command prompt and with a cd command enter the fontconvert directory.
```
cd /c/adafruit_gfx_library\fontconvert
```
Give the command:
```
make
```
This command will, eventually, create a "fontconvert.exe" file inside fontconvert directory.

#### STEP 4: Create your own font header files

Now that you have an executable file, you can use it to create your own fonts to work with Adafruit GFX lib.
So, if we suppose that you already have a .ttf file with your favorite fonts, jump to the command prompt and type:
```
./fontconvert yourfonts.ttf 9 > yourfonts9pt7b.h
```
You can read more details at: [learn.adafruit](https://learn.adafruit.com/adafruit-gfx-graphics-library/using-fonts).

Taraaaaaammm !! you've just created your new font header file. Put it inside the "Fonts" folder, grab a cup of coffee
and start playing with your Arduino (or whatever else ....)+ display module project.
