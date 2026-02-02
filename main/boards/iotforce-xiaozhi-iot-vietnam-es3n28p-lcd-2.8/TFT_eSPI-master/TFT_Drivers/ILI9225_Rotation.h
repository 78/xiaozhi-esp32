
// This is the command sequence that rotates the ILI9225 driver coordinate frame

  rotation = m % 4; // Limit the range of values to 0-3

  switch (rotation) {
    case 0:
    	writecommand(ILI9225_DRIVER_OUTPUT_CTRL);
	    writedata(0x01);writedata(0x1C);
      writecommand(ILI9225_ENTRY_MODE);
    	writedata(TFT_MAD_COLOR_ORDER);writedata(0x30);
      _width  = _init_width;
      _height = _init_height;
      break;
    case 1:
    	writecommand(ILI9225_DRIVER_OUTPUT_CTRL);
	    writedata(0x00);writedata(0x1C);
      writecommand(ILI9225_ENTRY_MODE);
    	writedata(TFT_MAD_COLOR_ORDER);writedata(0x38);
      _width  = _init_height;
      _height = _init_width;
      break;
    case 2:
    	writecommand(ILI9225_DRIVER_OUTPUT_CTRL);
	    writedata(0x02);writedata(0x1C);
      writecommand(ILI9225_ENTRY_MODE);
    	writedata(TFT_MAD_COLOR_ORDER);writedata(0x30);
      _width  = _init_width;
      _height = _init_height;
      break;
    case 3:
    	writecommand(ILI9225_DRIVER_OUTPUT_CTRL);
	    writedata(0x03);writedata(0x1C);
      writecommand(ILI9225_ENTRY_MODE);
    	writedata(TFT_MAD_COLOR_ORDER);writedata(0x38);
      _width  = _init_height;
      _height = _init_width;
      break;
  }
