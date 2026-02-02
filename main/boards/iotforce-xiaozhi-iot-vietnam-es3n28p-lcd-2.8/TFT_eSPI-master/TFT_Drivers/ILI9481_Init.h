
// This is the command sequence that initialises the ILI9481 driver
//
// This setup information uses simple 8-bit SPI writecommand() and writedata() functions
//
// See ST7735_Setup.h file for an alternative format

#define ILI9481_INIT_1 // Original default
//#define ILI9481_INIT_2 // CPT29
//#define ILI9481_INIT_3 // PVI35
//#define ILI9481_INIT_4 // AUO317
//#define ILI9481_INIT_5 // CMO35 *****
//#define ILI9481_INIT_6 // RGB
//#define ILI9481_INIT_7 // From #1774
//#define ILI9481_INIT_8 // From #1774

/////////////////////////////////////////////////////////////////////////////////////////
#ifdef ILI9481_INIT_1
// Configure ILI9481 display

    writecommand(TFT_SLPOUT);
    delay(20);

    writecommand(0xD0);
    writedata(0x07);
    writedata(0x42);
    writedata(0x18);

    writecommand(0xD1);
    writedata(0x00);
    writedata(0x07);
    writedata(0x10);

    writecommand(0xD2);
    writedata(0x01);
    writedata(0x02);

    writecommand(0xC0);
    writedata(0x10);
    writedata(0x3B);
    writedata(0x00);
    writedata(0x02);
    writedata(0x11);

    writecommand(0xC5);
    writedata(0x03);

    writecommand(0xC8);
    writedata(0x00);
    writedata(0x32);
    writedata(0x36);
    writedata(0x45);
    writedata(0x06);
    writedata(0x16);
    writedata(0x37);
    writedata(0x75);
    writedata(0x77);
    writedata(0x54);
    writedata(0x0C);
    writedata(0x00);

    writecommand(TFT_MADCTL);
    writedata(0x0A);

    writecommand(0x3A);
    #if defined (TFT_PARALLEL_8_BIT) || defined (TFT_PARALLEL_16_BIT) || defined (RPI_DISPLAY_TYPE)
      writedata(0x55);           // 16-bit colour interface
    #else
      writedata(0x66);           // 18-bit colour interface
    #endif
    
    #if !defined (TFT_PARALLEL_8_BIT) && !defined (TFT_PARALLEL_16_BIT)
      writecommand(TFT_INVON);
    #endif

    writecommand(TFT_CASET);
    writedata(0x00);
    writedata(0x00);
    writedata(0x01);
    writedata(0x3F);

    writecommand(TFT_PASET);
    writedata(0x00);
    writedata(0x00);
    writedata(0x01);
    writedata(0xDF);

    delay(120);
    writecommand(TFT_DISPON);

    delay(25);
// End of ILI9481 display configuration
/////////////////////////////////////////////////////////////////////////////////////////
#elif defined (ILI9481_INIT_2)
// Configure ILI9481 display

    writecommand(TFT_SLPOUT);
    delay(20);

    writecommand(0xD0);
    writedata(0x07);
    writedata(0x41);
    writedata(0x1D);

    writecommand(0xD1);
    writedata(0x00);
    writedata(0x2B);
    writedata(0x1F);

    writecommand(0xD2);
    writedata(0x01);
    writedata(0x11);

    writecommand(0xC0);
    writedata(0x10);
    writedata(0x3B);
    writedata(0x00);
    writedata(0x02);
    writedata(0x11);

    writecommand(0xC5);
    writedata(0x03);

    writecommand(0xC8);
    writedata(0x00);
    writedata(0x14);
    writedata(0x33);
    writedata(0x10);
    writedata(0x00);
    writedata(0x16);
    writedata(0x44);
    writedata(0x36);
    writedata(0x77);
    writedata(0x00);
    writedata(0x0F);
    writedata(0x00);

    writecommand(0xB0);
    writedata(0x00);

    writecommand(0xE4);
    writedata(0xA0);
    
    writecommand(0xF0);
    writedata(0x01);

    writecommand(0xF3);
    writedata(0x02);
    writedata(0x1A);

    writecommand(TFT_MADCTL);
    writedata(0x0A);

    writecommand(0x3A);
    #if defined (TFT_PARALLEL_8_BIT) || defined (TFT_PARALLEL_16_BIT) || defined (RPI_DISPLAY_TYPE)
      writedata(0x55);           // 16-bit colour interface
    #else
      writedata(0x66);           // 18-bit colour interface
    #endif
    
    #if !defined (TFT_PARALLEL_8_BIT) && !defined (TFT_PARALLEL_16_BIT)
      writecommand(TFT_INVON);
    #endif

    writecommand(TFT_CASET);
    writedata(0x00);
    writedata(0x00);
    writedata(0x01);
    writedata(0x3F);

    writecommand(TFT_PASET);
    writedata(0x00);
    writedata(0x00);
    writedata(0x01);
    writedata(0xDF);

    delay(120);
    writecommand(TFT_DISPON);

    delay(25);
// End of ILI9481 display configuration
/////////////////////////////////////////////////////////////////////////////////////////
#elif defined (ILI9481_INIT_3)
// Configure ILI9481 display

    writecommand(TFT_SLPOUT);
    delay(20);

    writecommand(0xD0);
    writedata(0x07);
    writedata(0x41);
    writedata(0x1D);

    writecommand(0xD1);
    writedata(0x00);
    writedata(0x2B);
    writedata(0x1F);

    writecommand(0xD2);
    writedata(0x01);
    writedata(0x11);

    writecommand(0xC0);
    writedata(0x10);
    writedata(0x3B);
    writedata(0x00);
    writedata(0x02);
    writedata(0x11);

    writecommand(0xC5);
    writedata(0x03);

    writecommand(0xC8);
    writedata(0x00);
    writedata(0x14);
    writedata(0x33);
    writedata(0x10);
    writedata(0x00);
    writedata(0x16);
    writedata(0x44);
    writedata(0x36);
    writedata(0x77);
    writedata(0x00);
    writedata(0x0F);
    writedata(0x00);

    writecommand(0xB0);
    writedata(0x00);

    writecommand(0xE4);
    writedata(0xA0);
    
    writecommand(0xF0);
    writedata(0x01);

    writecommand(0xF3);
    writedata(0x40);
    writedata(0x0A);

    writecommand(TFT_MADCTL);
    writedata(0x0A);

    writecommand(0x3A);
    #if defined (TFT_PARALLEL_8_BIT) || defined (TFT_PARALLEL_16_BIT) || defined (RPI_DISPLAY_TYPE)
      writedata(0x55);           // 16-bit colour interface
    #else
      writedata(0x66);           // 18-bit colour interface
    #endif
    
    #if !defined (TFT_PARALLEL_8_BIT) && !defined (TFT_PARALLEL_16_BIT)
      writecommand(TFT_INVON);
    #endif

    writecommand(TFT_CASET);
    writedata(0x00);
    writedata(0x00);
    writedata(0x01);
    writedata(0x3F);

    writecommand(TFT_PASET);
    writedata(0x00);
    writedata(0x00);
    writedata(0x01);
    writedata(0xDF);

    delay(120);
    writecommand(TFT_DISPON);

    delay(25);
// End of ILI9481 display configuration
/////////////////////////////////////////////////////////////////////////////////////////
#elif defined (ILI9481_INIT_4)
// Configure ILI9481 display

    writecommand(TFT_SLPOUT);
    delay(20);

    writecommand(0xD0);
    writedata(0x07);
    writedata(0x40);
    writedata(0x1D);

    writecommand(0xD1);
    writedata(0x00);
    writedata(0x18);
    writedata(0x13);

    writecommand(0xD2);
    writedata(0x01);
    writedata(0x11);

    writecommand(0xC0);
    writedata(0x10);
    writedata(0x3B);
    writedata(0x00);
    writedata(0x02);
    writedata(0x11);

    writecommand(0xC5);
    writedata(0x03);

    writecommand(0xC8);
    writedata(0x00);
    writedata(0x44);
    writedata(0x06);
    writedata(0x44);
    writedata(0x0A);
    writedata(0x08);
    writedata(0x17);
    writedata(0x33);
    writedata(0x77);
    writedata(0x44);
    writedata(0x08);
    writedata(0x0C);

    writecommand(0xB0);
    writedata(0x00);

    writecommand(0xE4);
    writedata(0xA0);
    
    writecommand(0xF0);
    writedata(0x01);

    writecommand(TFT_MADCTL);
    writedata(0x0A);

    writecommand(0x3A);
    #if defined (TFT_PARALLEL_8_BIT) || defined (TFT_PARALLEL_16_BIT) || defined (RPI_DISPLAY_TYPE)
      writedata(0x55);           // 16-bit colour interface
    #else
      writedata(0x66);           // 18-bit colour interface
    #endif
    
    #if !defined (TFT_PARALLEL_8_BIT) && !defined (TFT_PARALLEL_16_BIT)
      writecommand(TFT_INVON);
    #endif

    writecommand(TFT_CASET);
    writedata(0x00);
    writedata(0x00);
    writedata(0x01);
    writedata(0x3F);

    writecommand(TFT_PASET);
    writedata(0x00);
    writedata(0x00);
    writedata(0x01);
    writedata(0xDF);

    delay(120);
    writecommand(TFT_DISPON);

    delay(25);
// End of ILI9481 display configuration
/////////////////////////////////////////////////////////////////////////////////////////
#elif defined (ILI9481_INIT_5)
// Configure ILI9481 display

    writecommand(TFT_SLPOUT);
    delay(20);

    writecommand(0xD0);
    writedata(0x07);
    writedata(0x41);
    writedata(0x1D);

    writecommand(0xD1);
    writedata(0x00);
    writedata(0x1C);
    writedata(0x1F);

    writecommand(0xD2);
    writedata(0x01);
    writedata(0x11);

    writecommand(0xC0);
    writedata(0x10);
    writedata(0x3B);
    writedata(0x00);
    writedata(0x02);
    writedata(0x11);

    writecommand(0xC5);
    writedata(0x03);

    writecommand(0xC6);
    writedata(0x83);

    writecommand(0xC8);
    writedata(0x00);
    writedata(0x26);
    writedata(0x21);
    writedata(0x00);
    writedata(0x00);
    writedata(0x1F);
    writedata(0x65);
    writedata(0x23);
    writedata(0x77);
    writedata(0x00);
    writedata(0x0F);
    writedata(0x00);

    writecommand(0xB0);
    writedata(0x00);

    writecommand(0xE4);
    writedata(0xA0);
    
    writecommand(0xF0);
    writedata(0x01);

    writecommand(TFT_MADCTL);
    writedata(0x0A);

    writecommand(0x3A);
    #if defined (TFT_PARALLEL_8_BIT) || defined (TFT_PARALLEL_16_BIT) || defined (RPI_DISPLAY_TYPE)
      writedata(0x55);           // 16-bit colour interface
    #else
      writedata(0x66);           // 18-bit colour interface
    #endif
    
    #if !defined (TFT_PARALLEL_8_BIT) && !defined (TFT_PARALLEL_16_BIT)
      writecommand(TFT_INVON);
    #endif

    writecommand(TFT_CASET);
    writedata(0x00);
    writedata(0x00);
    writedata(0x01);
    writedata(0x3F);

    writecommand(TFT_PASET);
    writedata(0x00);
    writedata(0x00);
    writedata(0x01);
    writedata(0xDF);

    delay(120);
    writecommand(TFT_DISPON);

    delay(25);
// End of ILI9481 display configuration
/////////////////////////////////////////////////////////////////////////////////////////
#elif defined (ILI9481_INIT_6)
// Configure ILI9481 display

    writecommand(TFT_SLPOUT);
    delay(20);

    writecommand(0xD0);
    writedata(0x07);
    writedata(0x41);
    writedata(0x1D);

    writecommand(0xD1);
    writedata(0x00);
    writedata(0x2B);
    writedata(0x1F);

    writecommand(0xD2);
    writedata(0x01);
    writedata(0x11);

    writecommand(0xC0);
    writedata(0x10);
    writedata(0x3B);
    writedata(0x00);
    writedata(0x02);
    writedata(0x11);
    writedata(0x00);

    writecommand(0xC5);
    writedata(0x03);

    writecommand(0xC6);
    writedata(0x80);

    writecommand(0xC8);
    writedata(0x00);
    writedata(0x14);
    writedata(0x33);
    writedata(0x10);
    writedata(0x00);
    writedata(0x16);
    writedata(0x44);
    writedata(0x36);
    writedata(0x77);
    writedata(0x00);
    writedata(0x0F);
    writedata(0x00);

    writecommand(0xB0);
    writedata(0x00);

    writecommand(0xE4);
    writedata(0xA0);
    
    writecommand(0xF0);
    writedata(0x08);

    writecommand(0xF3);
    writedata(0x40);
    writedata(0x0A);

    writecommand(0xF6);
    writedata(0x84);

    writecommand(0xF7);
    writedata(0x80);

    writecommand(0xB3);
    writedata(0x00);
    writedata(0x01);
    writedata(0x06);
    writedata(0x30);

    writecommand(0xB4);
    writedata(0x00);

    writecommand(0x0C);
    writedata(0x00);
    writedata(0x55);

    writecommand(TFT_MADCTL);
    writedata(0x0A);

    writecommand(0x3A);
    #if defined (TFT_PARALLEL_8_BIT) || defined (TFT_PARALLEL_16_BIT) || defined (RPI_DISPLAY_TYPE)
      writedata(0x55);           // 16-bit colour interface
    #else
      writedata(0x66);           // 18-bit colour interface
    #endif
    
    #if !defined (TFT_PARALLEL_8_BIT) && !defined (TFT_PARALLEL_16_BIT)
      writecommand(TFT_INVON);
    #endif

    writecommand(TFT_CASET);
    writedata(0x00);
    writedata(0x00);
    writedata(0x01);
    writedata(0x3F);

    writecommand(TFT_PASET);
    writedata(0x00);
    writedata(0x00);
    writedata(0x01);
    writedata(0xDF);

    delay(120);
    writecommand(TFT_DISPON);

    delay(25);
// End of ILI9481 display configuration
/////////////////////////////////////////////////////////////////////////////////////////


// From #1774
#elif defined (ILI9481_INIT_7)
 ///ili9481+cmi3.5ips //效果不好
					//************* Start Initial Sequence **********//
					writecommand(0x11);
					delay(20);
					writecommand(0xD0);
					writedata(0x07);
					writedata(0x42);
					writedata(0x1B);

					writecommand(0xD1);
					writedata(0x00);
					writedata(0x14);
					writedata(0x1B);

					writecommand(0xD2);
					writedata(0x01);
					writedata(0x12);

					writecommand(0xC0);
					writedata(0x10);
					writedata(0x3B);
					writedata(0x00);
					writedata(0x02);
					writedata(0x01);

					writecommand(0xC5);
					writedata(0x03);

					writecommand(0xC8);
					writedata(0x00);
					writedata(0x46);
					writedata(0x44);
					writedata(0x50);
					writedata(0x04);
					writedata(0x16);
					writedata(0x33);
					writedata(0x13);
					writedata(0x77);
					writedata(0x05);
					writedata(0x0F);
					writedata(0x00);

					writecommand(0x36);
					writedata(0x0A);

					writecommand(0x3A);
    #if defined (TFT_PARALLEL_8_BIT) || defined (TFT_PARALLEL_16_BIT) || defined (RPI_DISPLAY_TYPE)
          writedata(0x55);           // 16-bit colour interface
    #else
          writedata(0x66);           // 18-bit colour interface
    #endif
    
    #if !defined (TFT_PARALLEL_8_BIT) && !defined (TFT_PARALLEL_16_BIT)
      writecommand(TFT_INVON);
    #endif

					writecommand(0x22);
					writedata(0x00);
					writedata(0x00);
					writedata(0x01);
					writedata(0x3F);

					writecommand(0x2B);
					writedata(0x00);
					writedata(0x00);
					writedata(0x01);
					writedata(0xE0);
					delay(120);
					writecommand(0x29);

#elif defined (ILI9481_INIT_8)

					//3.5IPS ILI9481+CMI	
					writecommand(0x01); //Soft_rese
					delay(220);

					writecommand(0x11);
					delay(280);

					writecommand(0xd0); //Power_Setting
					writedata(0x07);//07  VC[2:0] Sets the ratio factor of Vci to generate the reference voltages Vci1
					writedata(0x44);//41  BT[2:0] Sets the Step up factor and output voltage level from the reference voltages Vci1
					writedata(0x1E);//1f  17   1C  VRH[3:0]: Sets the factor to generate VREG1OUT from VCILVL
					delay(220);

					writecommand(0xd1); //VCOM Control
					writedata(0x00);//00
					writedata(0x0C);//1A   VCM [6:0] is used to set factor to generate VCOMH voltage from the reference voltage VREG1OUT  15    09
					writedata(0x1A);//1F   VDV[4:0] is used to set the VCOM alternating amplitude in the range of VREG1OUT x 0.70 to VREG1OUT   1F   18

					writecommand(0xC5);  //Frame Rate
					writedata(0x03); // 03   02

					writecommand(0xd2);  //Power_Setting for Normal Mode 
					writedata(0x01);  //01
					writedata(0x11);  //11

					writecommand(0xE4);  //?
					writedata(0xa0);
					writecommand(0xf3);
					writedata(0x00);
					writedata(0x2a);

					//1  OK
					writecommand(0xc8);
					writedata(0x00);
					writedata(0x26);
					writedata(0x21);
					writedata(0x00);
					writedata(0x00);
					writedata(0x1f);
					writedata(0x65);
					writedata(0x23);
					writedata(0x77);
					writedata(0x00);
					writedata(0x0f);
					writedata(0x00);
					//GAMMA SETTING

					writecommand(0xC0);	//Panel Driving Setting																          
					writedata(0x00); //1//00  REV  SM  GS
					writedata(0x3B); //2//NL[5:0]: Sets the number of lines to drive the LCD at an interval of 8 lines. 
					writedata(0x00); //3//SCN[6:0]
					writedata(0x02); //4//PTV: Sets the Vcom output in non-display area drive period
					writedata(0x11); //5//NDL: Sets the source output level in non-display area.  PTG: Sets the scan mode in non-display area.

					writecommand(0xc6); //Interface Control 
					writedata(0x83);
					//GAMMA SETTING 

					writecommand(0xf0); //?
					writedata(0x01);

					writecommand(0xE4);//?
					writedata(0xa0);

					//////倒装设置   NG
					writecommand(0x36);   
					writedata(0x0A); //  8C:出来两行   CA：出来一个点

					writecommand(0x3a);
    #if defined (TFT_PARALLEL_8_BIT) || defined (TFT_PARALLEL_16_BIT) || defined (RPI_DISPLAY_TYPE)
          writedata(0x55);           // 16-bit colour interface
    #else
          writedata(0x66);           // 18-bit colour interface
    #endif
    
    #if defined (TFT_PARALLEL_8_BIT) || defined (TFT_PARALLEL_16_BIT)
          writecommand(TFT_INVON);
    #endif

					writecommand(0xb4);//Display Mode and Frame Memory Write Mode Setting
					writedata(0x02);
					writedata(0x00); //?
					writedata(0x00);
					writedata(0x01);

					delay(280);

					writecommand(0x2a);
					writedata(0x00);
					writedata(0x00);
					writedata(0x01);
					writedata(0x3F); //3F

					writecommand(0x2b);
					writedata(0x00);
					writedata(0x00);
					writedata(0x01);
					writedata(0xDf); //DF

					//writecommand(0x21);
					writecommand(0x29);	
					writecommand(0x2c);

#endif