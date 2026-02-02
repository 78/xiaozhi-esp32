// Attach this header file to your sketch to use the GFX Free Fonts. You can write
// sketches without it, but it makes referencing them easier.

// This calls up ALL the fonts but they only get loaded if you actually
// use them in your sketch.
//
// No changes are needed to this header file unless new fonts are added to the
// library "Fonts/GFXFF" folder.
//
// To save a lot of typing long names, each font can easily be referenced in the
// sketch in three ways, either with:
//
//    1. Font file name with the & in front such as &FreeSansBoldOblique24pt7b
//       an example being:
//
//       tft.setFreeFont(&FreeSansBoldOblique24pt7b);
//
//    2. FF# where # is a number determined by looking at the list below
//       an example being:
//
//       tft.setFreeFont(FF32);
//
//    3. An abbreviation of the file name. Look at the list below to see
//       the abbreviations used, for example:
//
//       tft.setFreeFont(FSSBO24)
//
//       Where the letters mean:
//       F = Free font
//       M = Mono
//      SS = Sans Serif (double S to distinguish is form serif fonts)
//       S = Serif
//       B = Bold
//       O = Oblique (letter O not zero)
//       I = Italic
//       # =  point size, either 9, 12, 18 or 24
//
//  Setting the font to NULL will select the GLCD font:
//
//      tft.setFreeFont(NULL); // Set font to GLCD

#define LOAD_tftFF

#ifdef LOAD_tftFF // Only include the fonts if LOAD_tftFF is defined in User_Setup.h

// Use these when printing or drawing text in GLCD and high rendering speed fonts
#define GFXFF 1
#define GLCD  0
#define FONT2 2
#define FONT4 4
#define FONT6 6
#define FONT7 7
#define FONT8 8

// Use the following when calling setFont()
//
// Reserved for GLCD font  // FF0
//

#define TT1 &TomThumb

#define FM9 &FreeMono9pt7b
#define FM12 &FreeMono12pt7b
#define FM18 &FreeMono18pt7b
#define FM24 &FreeMono24pt7b

#define FMB9 &FreeMonoBold9pt7b
#define FMB12 &FreeMonoBold12pt7b
#define FMB18 &FreeMonoBold18pt7b
#define FMB24 &FreeMonoBold24pt7b

#define FMO9 &FreeMonoOblique9pt7b
#define FMO12 &FreeMonoOblique12pt7b
#define FMO18 &FreeMonoOblique18pt7b
#define FMO24 &FreeMonoOblique24pt7b

#define FMBO9 &FreeMonoBoldOblique9pt7b
#define FMBO12 &FreeMonoBoldOblique12pt7b
#define FMBO18 &FreeMonoBoldOblique18pt7b
#define FMBO24 &FreeMonoBoldOblique24pt7b

#define FSS9 &FreeSans9pt7b
#define FSS12 &FreeSans12pt7b
#define FSS18 &FreeSans18pt7b
#define FSS24 &FreeSans24pt7b

#define FSSB9 &FreeSansBold9pt7b
#define FSSB12 &FreeSansBold12pt7b
#define FSSB18 &FreeSansBold18pt7b
#define FSSB24 &FreeSansBold24pt7b

#define FSSO9 &FreeSansOblique9pt7b
#define FSSO12 &FreeSansOblique12pt7b
#define FSSO18 &FreeSansOblique18pt7b
#define FSSO24 &FreeSansOblique24pt7b

#define FSSBO9 &FreeSansBoldOblique9pt7b
#define FSSBO12 &FreeSansBoldOblique12pt7b
#define FSSBO18 &FreeSansBoldOblique18pt7b
#define FSSBO24 &FreeSansBoldOblique24pt7b

#define FS9 &FreeSerif9pt7b
#define FS12 &FreeSerif12pt7b
#define FS18 &FreeSerif18pt7b
#define FS24 &FreeSerif24pt7b

#define FSI9 &FreeSerifItalic9pt7b
#define FSI12 &FreeSerifItalic12pt7b
#define FSI19 &FreeSerifItalic18pt7b
#define FSI24 &FreeSerifItalic24pt7b

#define FSB9 &FreeSerifBold9pt7b
#define FSB12 &FreeSerifBold12pt7b
#define FSB18 &FreeSerifBold18pt7b
#define FSB24 &FreeSerifBold24pt7b

#define FSBI9 &FreeSerifBoldItalic9pt7b
#define FSBI12 &FreeSerifBoldItalic12pt7b
#define FSBI18 &FreeSerifBoldItalic18pt7b
#define FSBI24 &FreeSerifBoldItalic24pt7b

#define FF0 NULL //ff0 reserved for GLCD
#define FF1 &FreeMono9pt7b
#define FF2 &FreeMono12pt7b
#define FF3 &FreeMono18pt7b
#define FF4 &FreeMono24pt7b

#define FF5 &FreeMonoBold9pt7b
#define FF6 &FreeMonoBold12pt7b
#define FF7 &FreeMonoBold18pt7b
#define FF8 &FreeMonoBold24pt7b

#define FF9 &FreeMonoOblique9pt7b
#define FF10 &FreeMonoOblique12pt7b
#define FF11 &FreeMonoOblique18pt7b
#define FF12 &FreeMonoOblique24pt7b

#define FF13 &FreeMonoBoldOblique9pt7b
#define FF14 &FreeMonoBoldOblique12pt7b
#define FF15 &FreeMonoBoldOblique18pt7b
#define FF16 &FreeMonoBoldOblique24pt7b

#define FF17 &FreeSans9pt7b
#define FF18 &FreeSans12pt7b
#define FF19 &FreeSans18pt7b
#define FF20 &FreeSans24pt7b

#define FF21 &FreeSansBold9pt7b
#define FF22 &FreeSansBold12pt7b
#define FF23 &FreeSansBold18pt7b
#define FF24 &FreeSansBold24pt7b

#define FF25 &FreeSansOblique9pt7b
#define FF26 &FreeSansOblique12pt7b
#define FF27 &FreeSansOblique18pt7b
#define FF28 &FreeSansOblique24pt7b

#define FF29 &FreeSansBoldOblique9pt7b
#define FF30 &FreeSansBoldOblique12pt7b
#define FF31 &FreeSansBoldOblique18pt7b
#define FF32 &FreeSansBoldOblique24pt7b

#define FF33 &FreeSerif9pt7b
#define FF34 &FreeSerif12pt7b
#define FF35 &FreeSerif18pt7b
#define FF36 &FreeSerif24pt7b

#define FF37 &FreeSerifItalic9pt7b
#define FF38 &FreeSerifItalic12pt7b
#define FF39 &FreeSerifItalic18pt7b
#define FF40 &FreeSerifItalic24pt7b

#define FF41 &FreeSerifBold9pt7b
#define FF42 &FreeSerifBold12pt7b
#define FF43 &FreeSerifBold18pt7b
#define FF44 &FreeSerifBold24pt7b

#define FF45 &FreeSerifBoldItalic9pt7b
#define FF46 &FreeSerifBoldItalic12pt7b
#define FF47 &FreeSerifBoldItalic18pt7b
#define FF48 &FreeSerifBoldItalic24pt7b

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
// Now we define "s"tring versions for easy printing of the font name so:
//   tft.println(sFF5);
// will print
//   Mono bold 9
// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

#define sFF0 "GLCD"
#define sTT1 "Tom Thumb"
#define sFF1 "Mono 9"
#define sFF2 "Mono 12"
#define sFF3 "Mono 18"
#define sFF4 "Mono 24"

#define sFF5 "Mono bold 9"
#define sFF6 "Mono bold 12"
#define sFF7 "Mono bold 18"
#define sFF8 "Mono bold 24"

#define sFF9 "Mono oblique 9"
#define sFF10 "Mono oblique 12"
#define sFF11 "Mono oblique 18"
#define sFF12 "Mono oblique 24"

#define sFF13 "Mono bold oblique 9"
#define sFF14 "Mono bold oblique 12"
#define sFF15 "Mono bold oblique 18"
#define sFF16 "Mono bold oblique 24" // Full text line is too big for 480 pixel wide screen

#define sFF17 "Sans 9"
#define sFF18 "Sans 12"
#define sFF19 "Sans 18"
#define sFF20 "Sans 24"

#define sFF21 "Sans bold 9"
#define sFF22 "Sans bold 12"
#define sFF23 "Sans bold 18"
#define sFF24 "Sans bold 24"

#define sFF25 "Sans oblique 9"
#define sFF26 "Sans oblique 12"
#define sFF27 "Sans oblique 18"
#define sFF28 "Sans oblique 24"

#define sFF29 "Sans bold oblique 9"
#define sFF30 "Sans bold oblique 12"
#define sFF31 "Sans bold oblique 18"
#define sFF32 "Sans bold oblique 24"

#define sFF33 "Serif 9"
#define sFF34 "Serif 12"
#define sFF35 "Serif 18"
#define sFF36 "Serif 24"

#define sFF37 "Serif italic 9"
#define sFF38 "Serif italic 12"
#define sFF39 "Serif italic 18"
#define sFF40 "Serif italic 24"

#define sFF41 "Serif bold 9"
#define sFF42 "Serif bold 12"
#define sFF43 "Serif bold 18"
#define sFF44 "Serif bold 24"

#define sFF45 "Serif bold italic 9"
#define sFF46 "Serif bold italic 12"
#define sFF47 "Serif bold italic 18"
#define sFF48 "Serif bold italic 24"

#else // LOAD_tftFF not defined so setup defaults to prevent error messages

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
// Free fonts are not loaded in User_Setup.h so we must define all as font 1
// to prevent compile error messages
// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

#define GFXFF 1
#define GLCD  1
#define FONT2 2
#define FONT4 4
#define FONT6 6
#define FONT7 7
#define FONT8 8

#define FF0 1
#define FF1 1
#define FF2 1
#define FF3 1
#define FF4 1
#define FF5 1
#define FF6 1
#define FF7 1
#define FF8 1
#define FF9 1
#define FF10 1
#define FF11 1
#define FF12 1
#define FF13 1
#define FF14 1
#define FF15 1
#define FF16 1
#define FF17 1
#define FF18 1
#define FF19 1
#define FF20 1
#define FF21 1
#define FF22 1
#define FF23 1
#define FF24 1
#define FF25 1
#define FF26 1
#define FF27 1
#define FF28 1
#define FF29 1
#define FF30 1
#define FF31 1
#define FF32 1
#define FF33 1
#define FF34 1
#define FF35 1
#define FF36 1
#define FF37 1
#define FF38 1
#define FF39 1
#define FF40 1
#define FF41 1
#define FF42 1
#define FF43 1
#define FF44 1
#define FF45 1
#define FF46 1
#define FF47 1
#define FF48 1

#define FM9  1
#define FM12 1
#define FM18 1
#define FM24 1

#define FMB9  1
#define FMB12 1
#define FMB18 1
#define FMB24 1

#define FMO9  1
#define FMO12 1
#define FMO18 1
#define FMO24 1

#define FMBO9  1
#define FMBO12 1
#define FMBO18 1
#define FMBO24 1

#define FSS9  1
#define FSS12 1
#define FSS18 1
#define FSS24 1

#define FSSB9  1
#define FSSB12 1
#define FSSB18 1
#define FSSB24 1

#define FSSO9  1
#define FSSO12 1
#define FSSO18 1
#define FSSO24 1

#define FSSBO9  1
#define FSSBO12 1
#define FSSBO18 1
#define FSSBO24 1

#define FS9  1
#define FS12 1
#define FS18 1
#define FS24 1

#define FSI9  1
#define FSI12 1
#define FSI19 1
#define FSI24 1

#define FSB9  1
#define FSB12 1
#define FSB18 1
#define FSB24 1

#define FSBI9  1
#define FSBI12 1
#define FSBI18 1
#define FSBI24 1

#endif // LOAD_tftFF
