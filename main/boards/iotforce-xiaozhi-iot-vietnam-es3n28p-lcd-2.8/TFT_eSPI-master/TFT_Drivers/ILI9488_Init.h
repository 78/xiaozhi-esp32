
// This is the command sequence that initialises the ILI9488 driver
//
// This setup information uses simple 8-bit SPI writecommand() and writedata() functions
//
// See ST7735_Setup.h file for an alternative format


// Configure ILI9488 display

    writecommand(0xE0); // Positive Gamma Control
    writedata(0x00);
    writedata(0x03);
    writedata(0x09);
    writedata(0x08);
    writedata(0x16);
    writedata(0x0A);
    writedata(0x3F);
    writedata(0x78);
    writedata(0x4C);
    writedata(0x09);
    writedata(0x0A);
    writedata(0x08);
    writedata(0x16);
    writedata(0x1A);
    writedata(0x0F);

    writecommand(0XE1); // Negative Gamma Control
    writedata(0x00);
    writedata(0x16);
    writedata(0x19);
    writedata(0x03);
    writedata(0x0F);
    writedata(0x05);
    writedata(0x32);
    writedata(0x45);
    writedata(0x46);
    writedata(0x04);
    writedata(0x0E);
    writedata(0x0D);
    writedata(0x35);
    writedata(0x37);
    writedata(0x0F);

    writecommand(0XC0); // Power Control 1
    writedata(0x17);
    writedata(0x15);

    writecommand(0xC1); // Power Control 2
    writedata(0x41);

    writecommand(0xC5); // VCOM Control
    writedata(0x00);
    writedata(0x12);
    writedata(0x80);

    writecommand(TFT_MADCTL); // Memory Access Control
    writedata(0x48);          // MX, BGR

    writecommand(0x3A); // Pixel Interface Format
#if defined (TFT_PARALLEL_8_BIT) || defined (TFT_PARALLEL_16_BIT) || defined (RPI_DISPLAY_TYPE)
    writedata(0x55);  // 16-bit colour for parallel
#else
    writedata(0x66);  // 18-bit colour for SPI
#endif

    writecommand(0xB0); // Interface Mode Control
    writedata(0x00);

    writecommand(0xB1); // Frame Rate Control
    writedata(0xA0);

    writecommand(0xB4); // Display Inversion Control
    writedata(0x02);

    writecommand(0xB6); // Display Function Control
    writedata(0x02);
    writedata(0x02);
    writedata(0x3B);

    writecommand(0xB7); // Entry Mode Set
    writedata(0xC6);

    writecommand(0xF7); // Adjust Control 3
    writedata(0xA9);
    writedata(0x51);
    writedata(0x2C);
    writedata(0x82);

    writecommand(TFT_SLPOUT);  //Exit Sleep
delay(120);

    writecommand(TFT_DISPON);  //Display on
delay(25);

// End of ILI9488 display configuration



