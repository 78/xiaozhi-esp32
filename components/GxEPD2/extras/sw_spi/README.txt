### support for SW SPI for GxEPD2

GxEPD2 supports only HW SPI with the unmodified library.
The replacement files in the src directory provide the option to use SW SPI.
This is a workaround and not very elegant, therefore it is provided as a replacement only.

To use SW SPI with GxEPD2:
- copy the files GxEPD2_EPD.h and GxEPD2_EPD.cpp from the subdirectoy .src to the .src directory of the library.
- add the special call to the added init method BEFORE the normal init method:
  display.epd2.init(SW_SCK, SW_MOSI, 115200, true, 20, false); // define or replace SW_SCK, SW_MOSI
  display.init(115200); // needed to init upper level
  
To use the modified library with HW SPI
  - comment out the special init call:
  //display.epd2.init(SW_SCK, SW_MOSI, 115200, true, 20, false); // define or replace SW_SCK, SW_MOSI
  display.init(115200);

To switch back to using HW SPI during run time
  display.epd2.init(-1, -1, 115200, true, 20, false); // revert to HW SPI
  display.init(115200); // needed to init upper level

The SW SPI allows also to read through MOSI pin from DIN of e-paper controller
  data = display.epd2._readData();
  display.epd2._readData(data, n);