// This is the command sequence that initialises the ILI9225 driver

{
	writecommand(ILI9225_POWER_CTRL1);
	writedata(0x00);writedata(0x00);
	writecommand(ILI9225_POWER_CTRL2);
	writedata(0x00);writedata(0x00);
	writecommand(ILI9225_POWER_CTRL3);
	writedata(0x00);writedata(0x00);
	writecommand(ILI9225_POWER_CTRL4);
	writedata(0x00);writedata(0x00);
	writecommand(ILI9225_POWER_CTRL5);
	writedata(0x00);writedata(0x00);

	delay(40);

	writecommand(ILI9225_POWER_CTRL2);
	writedata(0x00);writedata(0x18);
	writecommand(ILI9225_POWER_CTRL3);
	writedata(0x61);writedata(0x21);
	writecommand(ILI9225_POWER_CTRL4);
	writedata(0x00);writedata(0x6F);
	writecommand(ILI9225_POWER_CTRL5);
	writedata(0x49);writedata(0x5F);
	writecommand(ILI9225_POWER_CTRL1);
	writedata(0x08);writedata(0x00);

	delay(10);

	writecommand(ILI9225_POWER_CTRL2);
	writedata(0x10);writedata(0x3B);

	delay(50);

	writecommand(ILI9225_LCD_AC_DRIVING_CTRL);
	writedata(0x01);writedata(0x00);
	writecommand(ILI9225_DISP_CTRL1);
	writedata(0x00);writedata(0x00);
	writecommand(ILI9225_BLANK_PERIOD_CTRL1);
	writedata(0x08);writedata(0x08);
	writecommand(ILI9225_FRAME_CYCLE_CTRL);
	writedata(0x11);writedata(0x00);
	writecommand(ILI9225_INTERFACE_CTRL);
	writedata(0x00);writedata(0x00);
	writecommand(ILI9225_OSC_CTRL);
	writedata(0x0D);writedata(0x01);
	writecommand(ILI9225_VCI_RECYCLING);
	writedata(0x00);writedata(0x20);
	writecommand(ILI9225_RAM_ADDR_SET1);
	writedata(0x00);writedata(0x00);
	writecommand(ILI9225_RAM_ADDR_SET2);
	writedata(0x00);writedata(0x00);

	writecommand(ILI9225_GATE_SCAN_CTRL);
	writedata(0x00);writedata(0x00);
	writecommand(ILI9225_VERTICAL_SCROLL_CTRL1);
	writedata(0x00);writedata(0xDB);
	writecommand(ILI9225_VERTICAL_SCROLL_CTRL2);
	writedata(0x00);writedata(0x00);
	writecommand(ILI9225_VERTICAL_SCROLL_CTRL3);
	writedata(0x00);writedata(0x00);
	writecommand(ILI9225_PARTIAL_DRIVING_POS1);
	writedata(0x00);writedata(0xDB);
	writecommand(ILI9225_PARTIAL_DRIVING_POS2);
	writedata(0x00);writedata(0x00);
	writecommand(ILI9225_HORIZONTAL_WINDOW_ADDR1);
	writedata(0x00);writedata(0xAF);
	writecommand(ILI9225_HORIZONTAL_WINDOW_ADDR2);
	writedata(0x00);writedata(0x00);
	writecommand(ILI9225_VERTICAL_WINDOW_ADDR1);
	writedata(0x00);writedata(0xDB);
	writecommand(ILI9225_VERTICAL_WINDOW_ADDR2);
	writedata(0x00);writedata(0x00);

	/* Set GAMMA curve */
	writecommand(ILI9225_GAMMA_CTRL1);
	writedata(0x00);writedata(0x00);
	writecommand(ILI9225_GAMMA_CTRL2);
	writedata(0x08);writedata(0x08);
	writecommand(ILI9225_GAMMA_CTRL3);
	writedata(0x08);writedata(0x0A);
	writecommand(ILI9225_GAMMA_CTRL4);
	writedata(0x00);writedata(0x0A);
	writecommand(ILI9225_GAMMA_CTRL5);
	writedata(0x0A);writedata(0x08);
	writecommand(ILI9225_GAMMA_CTRL6);
	writedata(0x08);writedata(0x08);
	writecommand(ILI9225_GAMMA_CTRL7);
	writedata(0x00);writedata(0x00);
	writecommand(ILI9225_GAMMA_CTRL8);
	writedata(0x0A);writedata(0x00);
	writecommand(ILI9225_GAMMA_CTRL9);
	writedata(0x07);writedata(0x10);
	writecommand(ILI9225_GAMMA_CTRL10);
	writedata(0x07);writedata(0x10);

	writecommand(ILI9225_DISP_CTRL1);
	writedata(0x00);writedata(0x12);

	delay(50);

	writecommand(ILI9225_DISP_CTRL1);
	writedata(0x10);writedata(0x17);

}