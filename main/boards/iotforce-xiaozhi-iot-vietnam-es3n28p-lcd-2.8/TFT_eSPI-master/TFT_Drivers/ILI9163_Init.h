
// This is the command sequence that initialises the ILI9163 driver
//
// This setup information is in a format accecpted by the commandList() function
// which reduces FLASH space, but on an ESP8266 there is plenty available!
//
// See ILI9341_Setup.h file for an alternative simpler format

{
	// Initialization commands for ILI9163 screens
	static const uint8_t ILI9163_cmds[] PROGMEM =
	{
	17,    // 17 commands follow
	0x01,  0 + TFT_INIT_DELAY, 120,  // Software reset
	0x11,  0 + TFT_INIT_DELAY, 5,    // Exit sleep mode
	0x3A,  1, 0x05, // Set pixel format
	0x26,  1, 0x04, // Set Gamma curve 3
	0xF2,  1, 0x01, // Gamma adjustment enabled
	0xE0, 15, 0x3F, 0x25, 0x1C, 0x1E, 0x20, 0x12, 0x2A, 0x90,
	          0x24, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, // Positive Gamma
	0xE1, 15, 0x20, 0x20, 0x20, 0x20, 0x05, 0x00, 0x15,0xA7,
	          0x3D, 0x18, 0x25, 0x2A, 0x2B, 0x2B, 0x3A, // Negative Gamma
	0xB1,  2, 0x08, 0x08, // Frame rate control 1
	0xB4,  1, 0x07,       // Display inversion
	0xC0,  2, 0x0A, 0x02, // Power control 1
	0xC1,  1, 0x02,       // Power control 2
	0xC5,  2, 0x50, 0x5B, // Vcom control 1
	0xC7,  1, 0x40,       // Vcom offset
	0x2A,  4, 0x00, 0x00, 0x00, 0x7F, // Set column address
	0x2B,  4 + TFT_INIT_DELAY, 0x00, 0x00, 0x00, 0x9F, 250, // Set page address
	0x36,  1, 0xC8,       // Set address mode
	0x29,  0,             // Set display on
	};

	commandList(ILI9163_cmds);

    #ifdef CGRAM_OFFSET
      colstart = 0;
      rowstart = 0;
    #endif
}