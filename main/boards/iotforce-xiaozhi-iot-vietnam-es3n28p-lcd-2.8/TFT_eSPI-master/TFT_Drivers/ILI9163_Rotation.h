
// This is the command sequence that rotates the ILI9163 driver coordinate frame

  rotation = m % 4;

  writecommand(TFT_MADCTL);
  switch (rotation) {
    case 0:
      writedata(TFT_MAD_MX | TFT_MAD_MY | TFT_MAD_BGR);
      _width  = _init_width;
      _height = _init_height;
	  #ifdef CGRAM_OFFSET
        colstart = 0;
        rowstart = 0;
	  #endif
      break;
    case 1:
      writedata(TFT_MAD_MV | TFT_MAD_MY | TFT_MAD_BGR);
      _width  = _init_height;
      _height = _init_width;
	  #ifdef CGRAM_OFFSET
        colstart = 0;
        rowstart = 0;
	  #endif
      break;
    case 2:
      writedata(TFT_MAD_BGR);
      _width  = _init_width;
      _height = _init_height;
	  #ifdef CGRAM_OFFSET
        colstart = 0;
        rowstart = 32;
	  #endif
      break;
    case 3:
      writedata(TFT_MAD_MX | TFT_MAD_MV | TFT_MAD_BGR);
      _width  = _init_height;
      _height = _init_width;
	  #ifdef CGRAM_OFFSET
        colstart = 32;
        rowstart = 0;
	  #endif
      break;
  }
