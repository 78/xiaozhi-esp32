
// This is the command sequence that rotates the SSD1351 driver coordinate frame

  rotation = m % 4; // Limit the range of values to 0-3

  uint8_t madctl = 0x64;

  switch (rotation) {
    case 0:
      madctl |= 0x10;
      _width  = _init_width;
      _height = _init_height;
      break;
    case 1:
      madctl |= 0x13;
      _width  = _init_height;
      _height = _init_width;
      break;
    case 2:
      madctl |= 0x02;
      _width  = _init_width;
      _height = _init_height;
      break;
    case 3:
      madctl |= 0x01;
      _width  = _init_height;
      _height = _init_width;
      break;
  }

  writecommand(0xA0); // SETREMAP
  writedata(madctl);
  writecommand(0xA1); // STARTLINE
  writedata(rotation < 2 ? TFT_HEIGHT : 0);
