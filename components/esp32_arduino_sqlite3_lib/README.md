# Sqlite3 Arduino library for ESP32

Note: This is a general purpose library based on the Sqlite codebase.  For logging sensor data into database please use [Sqlite Micro Logger](https://github.com/siara-cc/sqlite_micro_logger_arduino), which is faster and memory efficient.

This library enables access to SQLite database files from SPIFFS or SD Cards through ESP32 SoC.  Given below is a picture of a board that has a ready-made Micro SD slot (using SDMMC 4 bit mode - see example sqlite3_sdmmc):

![](ESP_WROOM_32_breakout.png?raw=true)

Also shown below is the wiring between ESP-WROOM-32 breakout board and Micro SD Shield (using SPI mode - see example sqlite3_sdspi):

![](ESP32_MSD_Shield_Wiring.jpg?raw=true)

## Why Sqlite on ESP32 is exciting?

[Sqlite3](http://sqlite.org) is the favourite database of all that is portable and widely used.  Availability on ESP32 platform makes it even more portable.  Sqlite can handle terrabyte sized data, ACID compliant and guaranteed to be stable.

So far IoT systems could offer SD Card access, which enabled gigabyte size files accessible, but was not fully utilized because the libraries that were available so far (such as [Arduino Extended Database Library](https://github.com/jwhiddon/EDB) or [SimpleDB](http://www.kendziorra.nl/arduino/103-simpledb-simple-flexible-and-smal)) offered only record number based or linear search and are terribly slow for key based indexed search.

Sqlite stores data in [B+Tree pages](https://en.wikipedia.org/wiki/B%2B_tree) and so can locate data from millions of records using variable length keys without exerting stress on CPU or RAM.  This is demonstrated using the sample databases provided in the example sections.

Even with the 500 odd kilbytes RAM available on ESP32, millions of records and gigabyte sized databases can be accessed.

## Usage

Sqlite3 C API such as `sqlite3_open` can be directly invoked. Before calling please invoke:

```c++
   SD_MMC.begin(); // for Cards attached to the High speed 4-bit port 
   SPI.begin(); SD.begin(); // for Cards attached to the SPI bus
   SPIFFS.begin(); // For SPIFFS
```
as appropriate.

The ESP32 Arduino library has an excellent VFS layer.  Even multiple cards can be supported on the SPI bus by specifying the pin number and mount point using the `begin()` method.

The default mount points are:
```c++
   '/sdcard' // for SD_MMC 
   '/sd' // for SD on SPI
   '/spiffs' // For SPIFFS
```

and the filenames are to be prefixed with these paths in the `sqlite3_open()` function (such as `sqlite3_open("/spiffs/my.db")`).

Please see the examples for full illustration of usage for the different file systems. The sample databases given (under `examples/sqlite3_sdmmc/data` folder) need to be copied to the Micro SD card root folder before the SD examples can be used.  Please see the comments section of the example.

## Wiring

While there is no wiring needed for SPIFFS, for attaching cards to SPI bus, please use the following connections:

```c++
 * SD Card    |  ESP32
 *  DAT2 (1)      -
 *  DAT3 (2)      SS (D5)
 *  CMD  (3)      MOSI (D23)
 *  VDD  (4)      3.3V
 *  CLK  (5)      SCK (D19)
 *  VSS  (6)      GND
 *  DAT0 (7)      MISO (D18)
 *  DAT1 (8)      -
```

And for SD card attached to High-speed 4-bit SD_MMC port, use:

```c++
 * SD Card    |  ESP32
 *  DAT2 (1)      D12
 *  DAT3 (2)      D13
 *  CMD  (3)      D15
 *  VDD  (4)      3.3V
 *  CLK  (5)      D14
 *  VSS  (6)      GND
 *  DAT0 (7)      D2
 *  DAT1 (8)      D4
```

If you are using a board such as shown in the picture above, this wiring is ready-made.

## Installation

Please download this library, unzip it to the libraries folder of your ESP32 sdk location. The location varies according to your OS.  For example, it is usually found in the following locations:
```
Windows: C:\Users\(username)\AppData\Roaming\Arduino15
Linux: /home/<username>/.arduino15
MacOS: /home/<username>/Library/Arduino15
```
Under Arduino15 folder please navigate to `packages/esp32/hardware/esp32/<version>/libraries`

If you do not have the ESP32 sdk for Arduino, please see https://github.com/espressif/arduino-esp32 for installing it.

## Dependencies / pre-requisites

No dependencies except for the Arduino and ESP32 core SDK. The Sqlite3 code is included with the library.

## Limitations on ESP32

* No serious limitations, except its a bit slow on large datasets. It takes around 700 ms to retrieve from a dataset containing 10 million rows, even using the index.

## Limitations of this library

* Locking is not implemented.  So it cannot be reliably used in a multi-threaded / multi-core code set, except for read-only operations.

## Limitations of Flash memory

Any Flash memory such as those available on SPIFFS or Micro SD cards have limitation on number of writes / erase per sector.  Usually the limitation is 10000 writes or 100000 writes (on the same sector).  Although ESP32 supports wear-levelling,  this is to be kept in mind before venturing into write-intensive database projects.  There is no limitation on reading from Flash.

## Compression with Shox96

(Shox96 is deprecated and Unishox explained below will be supported in future).

This implementation of `sqlite3` includes two functions `shox96_0_2c()` and `shox96_0_2d()` for compressing and decompressing text data.

Shox96 is a compression technique developed for reducing storage size of Short Strings. Details of how it works can be found [here](https://github.com/siara-cc/Shox96).

As of now it can work on only strings made of 'A to Z', 'a to z', '0-9', Special Characters such as &*() etc. found on keyboard, CR, LF, TAB and Space.

In general it can achieve upto 40% size reduction for Short Strings.

### Usage

The following set of commands demonstrate how compression can be accomplished:

```sql
create table test (b1 blob);
insert into test values (shox96_0_2c('Hello World'));
insert into test values (shox96_0_2c('Lorem Ipsum is simply dummy text of the printing and typesetting industry. Lorem Ipsum has been the industry''s standard dummy text ever since the 1500s, when an unknown printer took a galley of type and scrambled it to make a type specimen book.'));
select txt, length(txt) txt_len from (select shox96_0_2d(b1) txt from test);
select length(b1) compressed_len from test;
```

See screenshots section for output.

### Limitations (for Shox96)

- Trying to decompress any blob that was not compressed using `shox96_0_2c()` will crash the program.
- It does not work if the string has binary characters. that is, other than ASCII 32 to 126, CR, LF and Tab.
- Dictionary based compression / decompression is not yet implemented.

## Compression with Unishox

This implementation also includes two functions `unishox1c()` and `unishox1d()` for compressing and decompressing text data.

Unishox is a compression technique developed for reducing storage size of Short Unicode Strings. Details of how it works can be found [here](https://github.com/siara-cc/Unishox).

In general it can achieve upto 40% size reduction for Short Strings.

### Usage

The usage is similar to that of Shox96, only in this case UTF-8 strings can be used.

See screenshots section for output.

### Limitations (for Unishox)

- Trying to decompress any blob that was not compressed using `unishox1c()` will crash the program.

## Acknowledgements

* This library was developed based on NodeMCU module developed by [Luiz Felipe Silva](https://github.com/luizfeliperj). The documentation can be found [here](https://nodemcu.readthedocs.io/en/master/en/modules/sqlite3/).
* The census2000 and baby names databases were taken from here: http://2016.padjo.org/tutorials/sqlite-data-starterpacks/. But no license information is available.
* The mdr512.db (Million Domain Rank database) was created with data from [The Majestic Million](https://majestic.com/reports/majestic-million) and is provided under CC 3.0 Attribution license.
* The [ESP32 core for Arduino](https://github.com/espressif/arduino-esp32)
* [The Arduino platform](https://arduino.cc)

## Screenshots

### Output of Micro SD example

![](output_screenshot.png?raw=true)

### Output of SD Card database query through WebServer example

![](output_web_1.png?raw=true)
![](output_web_2.png?raw=true)

### SQLite console

![](console_screenshot.png?raw=true)

### Unishox compression

![](output_web_console.png?raw=true)

### Output of Querying StackOverflow DB through WebServer example:

![](output_web_so.png?raw=true)
![](output_web_so_id.png?raw=true)
![](output_web_so_name.png?raw=true)
![](output_web_so_loc.png?raw=true)

## Issues

Please contact the author or create issue here if you face problems.
