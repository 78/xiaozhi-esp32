
// This is the command sequence that initialises the ST7796 driver
//
// This setup information uses simple 8-bit SPI writecommand() and writedata() functions
//
// See ST7735_Setup.h file for an alternative format

#define TFT_INIT_DELAY 0
{
	delay(120);

	writecommand(0x01); //Software reset
	delay(120);

	writecommand(0x11); //Sleep exit                                            
	delay(120);

	writecommand(0xF0); //Command Set control                                 
	writedata(0xC3);    //Enable extension command 2 partI
	
	writecommand(0xF0); //Command Set control                                 
	writedata(0x96);    //Enable extension command 2 partII
	
	writecommand(0x36); //Memory Data Access Control MX, MY, RGB mode                                    
	writedata(0x48);    //X-Mirror, Top-Left to right-Buttom, RGB  
	
	writecommand(0x3A); //Interface Pixel Format                                    
	writedata(0x55);    //Control interface color format set to 16
	
	
	writecommand(0xB4); //Column inversion 
	writedata(0x01);    //1-dot inversion

	writecommand(0xB6); //Display Function Control
	writedata(0x80);    //Bypass
	writedata(0x02);    //Source Output Scan from S1 to S960, Gate Output scan from G1 to G480, scan cycle=2
	writedata(0x3B);    //LCD Drive Line=8*(59+1)


	writecommand(0xE8); //Display Output Ctrl Adjust
	writedata(0x40);
	writedata(0x8A);	
	writedata(0x00);
	writedata(0x00);
	writedata(0x29);    //Source eqaulizing period time= 22.5 us
	writedata(0x19);    //Timing for "Gate start"=25 (Tclk)
	writedata(0xA5);    //Timing for "Gate End"=37 (Tclk), Gate driver EQ function ON
	writedata(0x33);
	
	writecommand(0xC1); //Power control2                          
	writedata(0x06);    //VAP(GVDD)=3.85+( vcom+vcom offset), VAN(GVCL)=-3.85+( vcom+vcom offset)
	 
	writecommand(0xC2); //Power control 3                                      
	writedata(0xA7);    //Source driving current level=low, Gamma driving current level=High
	 
	writecommand(0xC5); //VCOM Control
	writedata(0x18);    //VCOM=0.9

	delay(120);
	
	//ST7796 Gamma Sequence
	writecommand(0xE0); //Gamma"+"                                             
	writedata(0xF0);
	writedata(0x09); 
	writedata(0x0b);
	writedata(0x06); 
	writedata(0x04);
	writedata(0x15); 
	writedata(0x2F);
	writedata(0x54); 
	writedata(0x42);
	writedata(0x3C); 
	writedata(0x17);
	writedata(0x14);
	writedata(0x18); 
	writedata(0x1B); 
	 
	writecommand(0xE1); //Gamma"-"                                             
	writedata(0xE0);
	writedata(0x09); 
	writedata(0x0B);
	writedata(0x06); 
	writedata(0x04);
	writedata(0x03); 
	writedata(0x2B);
	writedata(0x43); 
	writedata(0x42);
	writedata(0x3B); 
	writedata(0x16);
	writedata(0x14);
	writedata(0x17); 
	writedata(0x1B);

  delay(120);
	
	writecommand(0xF0); //Command Set control                                 
	writedata(0x3C);    //Disable extension command 2 partI

	writecommand(0xF0); //Command Set control                                 
	writedata(0x69);    //Disable extension command 2 partII

  end_tft_write();
  delay(120);
  begin_tft_write();

	writecommand(0x29); //Display on                                          	
}