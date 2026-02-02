
// This is the command sequence that initialises the ILI9341 driver
//
// This setup information uses simple 8-bit SPI writecommand() and writedata() functions
//
// See ST7735_Setup.h file for an alternative format

#if defined (ILI9341_DRIVER) || defined (ILI9342_DRIVER)
{
  writecommand(0xEF);
  writedata(0x03);
  writedata(0x80);
  writedata(0x02);

  writecommand(0xCF);
  writedata(0x00);
  writedata(0XC1);
  writedata(0X30);

  writecommand(0xED);
  writedata(0x64);
  writedata(0x03);
  writedata(0X12);
  writedata(0X81);

  writecommand(0xE8);
  writedata(0x85);
  writedata(0x00);
  writedata(0x78);

  writecommand(0xCB);
  writedata(0x39);
  writedata(0x2C);
  writedata(0x00);
  writedata(0x34);
  writedata(0x02);

  writecommand(0xF7);
  writedata(0x20);

  writecommand(0xEA);
  writedata(0x00);
  writedata(0x00);

  writecommand(ILI9341_PWCTR1);    //Power control
  writedata(0x23);   //VRH[5:0]

  writecommand(ILI9341_PWCTR2);    //Power control
  writedata(0x10);   //SAP[2:0];BT[3:0]

  writecommand(ILI9341_VMCTR1);    //VCM control
  writedata(0x3e);
  writedata(0x28);

  writecommand(ILI9341_VMCTR2);    //VCM control2
  writedata(0x86);  //--

  writecommand(ILI9341_MADCTL);    // Memory Access Control
#ifdef M5STACK
  writedata(TFT_MAD_MY | TFT_MAD_MV | TFT_MAD_COLOR_ORDER); // Rotation 0 (portrait mode)
#else
  writedata(TFT_MAD_MX | TFT_MAD_COLOR_ORDER); // Rotation 0 (portrait mode)
#endif

  writecommand(ILI9341_PIXFMT);
  writedata(0x55);

  writecommand(ILI9341_FRMCTR1);
  writedata(0x00);
  writedata(0x13); // 0x18 79Hz, 0x1B default 70Hz, 0x13 100Hz

  writecommand(ILI9341_DFUNCTR);    // Display Function Control
  writedata(0x08);
  writedata(0x82);
  writedata(0x27);

  writecommand(0xF2);    // 3Gamma Function Disable
  writedata(0x00);

  writecommand(ILI9341_GAMMASET);    //Gamma curve selected
  writedata(0x01);

  writecommand(ILI9341_GMCTRP1);    //Set Gamma
  writedata(0x0F);
  writedata(0x31);
  writedata(0x2B);
  writedata(0x0C);
  writedata(0x0E);
  writedata(0x08);
  writedata(0x4E);
  writedata(0xF1);
  writedata(0x37);
  writedata(0x07);
  writedata(0x10);
  writedata(0x03);
  writedata(0x0E);
  writedata(0x09);
  writedata(0x00);

  writecommand(ILI9341_GMCTRN1);    //Set Gamma
  writedata(0x00);
  writedata(0x0E);
  writedata(0x14);
  writedata(0x03);
  writedata(0x11);
  writedata(0x07);
  writedata(0x31);
  writedata(0xC1);
  writedata(0x48);
  writedata(0x08);
  writedata(0x0F);
  writedata(0x0C);
  writedata(0x31);
  writedata(0x36);
  writedata(0x0F);

  writecommand(ILI9341_SLPOUT);    //Exit Sleep
 
  end_tft_write();
  delay(120);
  begin_tft_write();
  
  writecommand(ILI9341_DISPON);    //Display on

}

#elif defined (ILI9341_2_DRIVER) // Alternative init sequence, see https://github.com/Bodmer/TFT_eSPI/issues/1172

{
writecommand(0xCF);
writedata(0x00);
writedata(0XC1);
writedata(0X30);

writecommand(0xED);
writedata(0x64);
writedata(0x03);
writedata(0X12);
writedata(0X81);

writecommand(0xE8);
writedata(0x85);
writedata(0x00);
writedata(0x78);

writecommand(0xCB);
writedata(0x39);
writedata(0x2C);
writedata(0x00);
writedata(0x34);
writedata(0x02);

writecommand(0xF7);
writedata(0x20);

writecommand(0xEA);
writedata(0x00);
writedata(0x00);

writecommand(ILI9341_PWCTR1); //Power control
writedata(0x10); //VRH[5:0]

writecommand(ILI9341_PWCTR2); //Power control
writedata(0x00); //SAP[2:0];BT[3:0]

writecommand(ILI9341_VMCTR1); //VCM control
writedata(0x30);
writedata(0x30);

writecommand(ILI9341_VMCTR2); //VCM control2
writedata(0xB7); //--

writecommand(ILI9341_PIXFMT);
writedata(0x55);

writecommand(0x36); // Memory Access Control
writedata(0x08); // Rotation 0 (portrait mode)

writecommand(ILI9341_FRMCTR1);
writedata(0x00);
writedata(0x1A);

writecommand(ILI9341_DFUNCTR); // Display Function Control
writedata(0x08);
writedata(0x82);
writedata(0x27);

writecommand(0xF2); // 3Gamma Function Disable
writedata(0x00);

writecommand(0x26); //Gamma curve selected
writedata(0x01);

writecommand(0xE0); //Set Gamma
writedata(0x0F);
writedata(0x2A);
writedata(0x28);
writedata(0x08);
writedata(0x0E);
writedata(0x08);
writedata(0x54);
writedata(0xA9);
writedata(0x43);
writedata(0x0A);
writedata(0x0F);
writedata(0x00);
writedata(0x00);
writedata(0x00);
writedata(0x00);

writecommand(0XE1); //Set Gamma
writedata(0x00);
writedata(0x15);
writedata(0x17);
writedata(0x07);
writedata(0x11);
writedata(0x06);
writedata(0x2B);
writedata(0x56);
writedata(0x3C);
writedata(0x05);
writedata(0x10);
writedata(0x0F);
writedata(0x3F);
writedata(0x3F);
writedata(0x0F);

writecommand(0x2B);
writedata(0x00);
writedata(0x00);
writedata(0x01);
writedata(0x3f);

writecommand(0x2A);
writedata(0x00);
writedata(0x00);
writedata(0x00);
writedata(0xef);

writecommand(ILI9341_SLPOUT); //Exit Sleep

end_tft_write();
delay(120);
begin_tft_write();

writecommand(ILI9341_DISPON); //Display on
}
#endif