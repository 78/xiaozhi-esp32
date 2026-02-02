// Demo based on:
// UTFT_Demo_320x240 by Henning Karlsen
// web: http://www.henningkarlsen.com/electronics
//
/*

 This sketch uses the GLCD and font 2 only.
 
 Make sure all the display driver and pin connections are correct by
 editing the User_Setup.h file in the TFT_eSPI library folder.

 #########################################################################
 ###### DON'T FORGET TO UPDATE THE User_Setup.h FILE IN THE LIBRARY ######
 #########################################################################
 */

#include "SPI.h"

#include "TFT_eSPI.h"

#define TFT_GREY 0x7BEF

TFT_eSPI myGLCD = TFT_eSPI();       // Invoke custom library

unsigned long runTime = 0;
void setup()
{
  randomSeed(analogRead(A0));
// Setup the LCD
  myGLCD.init();
  myGLCD.setRotation(1);
}

void loop()
{
  randomSeed(millis());
  //randomSeed(1234); // This ensure test is repeatable with exact same draws each loop
  int buf[318];
  int x, x2;
  int y, y2;
  int r;
  runTime = millis();
// Clear the screen and draw the frame
  myGLCD.fillScreen(TFT_BLACK);


  myGLCD.fillRect(0, 0, 319, 14,TFT_RED);

  myGLCD.fillRect(0, 226, 319, 14,TFT_GREY);

  myGLCD.setTextColor(TFT_BLACK,TFT_RED);
  myGLCD.drawCentreString("* TFT_eSPI *", 160, 4, 1);
  myGLCD.setTextColor(TFT_YELLOW,TFT_GREY);
  myGLCD.drawCentreString("Adapted by Bodmer", 160, 228,1);

  myGLCD.drawRect(0, 14, 319, 211, TFT_BLUE);

// Draw crosshairs
  myGLCD.drawLine(159, 15, 159, 224,TFT_BLUE);
  myGLCD.drawLine(1, 119, 318, 119,TFT_BLUE);
  for (int i=9; i<310; i+=10)
    myGLCD.drawLine(i, 117, i, 121,TFT_BLUE);
  for (int i=19; i<220; i+=10)
    myGLCD.drawLine(157, i, 161, i,TFT_BLUE);

// Draw sin-, cos- and tan-lines  
  myGLCD.setTextColor(TFT_CYAN);
  myGLCD.drawString("Sin", 5, 15,2);
  for (int i=1; i<318; i++)
  {
    myGLCD.drawPixel(i,119+(sin(((i*1.13)*3.14)/180)*95),TFT_CYAN);
  }
  myGLCD.setTextColor(TFT_RED);
  myGLCD.drawString("Cos", 5, 30,2);
  for (int i=1; i<318; i++)
  {
    myGLCD.drawPixel(i,119+(cos(((i*1.13)*3.14)/180)*95),TFT_RED);
  }
  myGLCD.setTextColor(TFT_YELLOW);
  myGLCD.drawString("Tan", 5, 45,2);
  for (int i=1; i<318; i++)
  {
    myGLCD.drawPixel(i,119+(tan(((i*1.13)*3.14)/180)),TFT_YELLOW);
  }

  delay(0);

  myGLCD.fillRect(1,15,317,209,TFT_BLACK);

  myGLCD.drawLine(159, 15, 159, 224,TFT_BLUE);
  myGLCD.drawLine(1, 119, 318, 119,TFT_BLUE);
int col = 0;
// Draw a moving sinewave
  x=1;
  for (int i=1; i<(317*20); i++) 
  {
    x++;
    if (x==318)
      x=1;
    if (i>318)
    {
      if ((x==159)||(buf[x-1]==119))
        col = TFT_BLUE;
      else
      myGLCD.drawPixel(x,buf[x-1],TFT_BLACK);
    }
    y=119+(sin(((i*1.1)*3.14)/180)*(90-(i / 100)));
    myGLCD.drawPixel(x,y,TFT_BLUE);
    buf[x-1]=y;
  }

  delay(0);

  myGLCD.fillRect(1,15,317,209,TFT_BLACK);

// Draw some filled rectangles
  for (int i=1; i<6; i++)
  {
    switch (i)
    {
      case 1:
        col = TFT_MAGENTA;
        break;
      case 2:
        col = TFT_RED;
        break;
      case 3:
        col = TFT_GREEN;
        break;
      case 4:
        col = TFT_BLUE;
        break;
      case 5:
        col = TFT_YELLOW;
        break;
    }
    myGLCD.fillRect(70+(i*20), 30+(i*20), 60, 60,col);
  }

  delay(0);

  myGLCD.fillRect(1,15,317,209,TFT_BLACK);

// Draw some filled, rounded rectangles
  for (int i=1; i<6; i++)
  {
    switch (i)
    {
      case 1:
        col = TFT_MAGENTA;
        break;
      case 2:
        col = TFT_RED;
        break;
      case 3:
        col = TFT_GREEN;
        break;
      case 4:
        col = TFT_BLUE;
        break;
      case 5:
        col = TFT_YELLOW;
        break;
    }
    myGLCD.fillRoundRect(190-(i*20), 30+(i*20), 60,60, 3,col);
  }
  
  delay(0);

  myGLCD.fillRect(1,15,317,209,TFT_BLACK);

// Draw some filled circles
  for (int i=1; i<6; i++)
  {
    switch (i)
    {
      case 1:
        col = TFT_MAGENTA;
        break;
      case 2:
        col = TFT_RED;
        break;
      case 3:
        col = TFT_GREEN;
        break;
      case 4:
        col = TFT_BLUE;
        break;
      case 5:
        col = TFT_YELLOW;
        break;
    }
    myGLCD.fillCircle(100+(i*20),60+(i*20), 30,col);
  }
  
  delay(0);

  myGLCD.fillRect(1,15,317,209,TFT_BLACK);

// Draw some lines in a pattern

  for (int i=15; i<224; i+=5)
  {
    myGLCD.drawLine(1, i, (i*1.44)-10, 223,TFT_RED);
  }

  for (int i=223; i>15; i-=5)
  {
    myGLCD.drawLine(317, i, (i*1.44)-11, 15,TFT_RED);
  }

  for (int i=223; i>15; i-=5)
  {
    myGLCD.drawLine(1, i, 331-(i*1.44), 15,TFT_CYAN);
  }

  for (int i=15; i<224; i+=5)
  {
    myGLCD.drawLine(317, i, 330-(i*1.44), 223,TFT_CYAN);
  }
  
  delay(0);


  myGLCD.fillRect(1,15,317,209,TFT_BLACK);

// Draw some random circles
  for (int i=0; i<100; i++)
  {
    x=32+random(256);
    y=45+random(146);
    r=random(30);
    myGLCD.drawCircle(x, y, r,random(0xFFFF));
  }

  delay(0);

  myGLCD.fillRect(1,15,317,209,TFT_BLACK);

// Draw some random rectangles
  for (int i=0; i<100; i++)
  {
    x=2+random(316);
    y=16+random(207);
    x2=2+random(316);
    y2=16+random(207);
    if (x2<x) {
      r=x;x=x2;x2=r;
    }
    if (y2<y) {
      r=y;y=y2;y2=r;
    }
    myGLCD.drawRect(x, y, x2-x, y2-y,random(0xFFFF));
  }

  delay(0);


  myGLCD.fillRect(1,15,317,209,TFT_BLACK);

// Draw some random rounded rectangles
  for (int i=0; i<100; i++)
  {
    x=2+random(316);
    y=16+random(207);
    x2=2+random(316);
    y2=16+random(207);
    // We need to get the width and height and do some window checking
    if (x2<x) {
      r=x;x=x2;x2=r;
    }
    if (y2<y) {
      r=y;y=y2;y2=r;
    }
    // We need a minimum size of 6
    if((x2-x)<6) x2=x+6;
    if((y2-y)<6) y2=y+6;
    myGLCD.drawRoundRect(x, y, x2-x,y2-y, 3,random(0xFFFF));
  }

  delay(0);

  myGLCD.fillRect(1,15,317,209,TFT_BLACK);

 //randomSeed(1234);
 int colour = 0;
 for (int i=0; i<100; i++)
  {
    x=2+random(316);
    y=16+random(209);
    x2=2+random(316);
    y2=16+random(209);
    colour=random(0xFFFF);
    myGLCD.drawLine(x, y, x2, y2,colour);
  }

  delay(0);

  myGLCD.fillRect(1,15,317,209,TFT_BLACK);

  // This test has been modified as it takes more time to calculate the random numbers
  // than to draw the pixels (3 seconds to produce 30,000 randoms)!
  for (int i=0; i<10000; i++)
  {
    myGLCD.drawPixel(2+random(316), 16+random(209),random(0xFFFF));
  }

  // Draw 10,000 pixels to fill a 100x100 pixel box
  // use the coords as the colour to produce the banding
  //byte i = 100;
  //while (i--) {
  //  byte j = 100;
  //  while (j--) myGLCD.drawPixel(i+110,j+70,i+j);
  //  //while (j--) myGLCD.drawPixel(i+110,j+70,0xFFFF);
  //}
  delay(0);

  myGLCD.fillScreen(TFT_BLUE);
  myGLCD.fillRoundRect(80, 70, 239-80,169-70, 3,TFT_RED);
  
  myGLCD.setTextColor(TFT_WHITE,TFT_RED);
  myGLCD.drawCentreString("That's it!", 160, 93,2);
  myGLCD.drawCentreString("Restarting in a", 160, 119,2);
  myGLCD.drawCentreString("few seconds...", 160, 132,2);

  runTime = millis()-runTime;
  myGLCD.setTextColor(TFT_GREEN,TFT_BLUE);
  myGLCD.drawCentreString("Runtime: (msecs)", 160, 210,2);
  myGLCD.setTextDatum(TC_DATUM);
  myGLCD.drawNumber(runTime, 160, 225,2);
  delay (5000);
}


