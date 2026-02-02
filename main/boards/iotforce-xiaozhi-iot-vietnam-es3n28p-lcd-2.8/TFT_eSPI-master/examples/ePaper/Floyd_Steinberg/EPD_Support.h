/*  Support definitions and functions for ePaper examples
 *  These tailor the library and screen settings
 *  Must be a header file to ensure #defines are established first
 *  
 *  Created by Bodmer 30/3/18 for TFT_eSPI library:
 *  https://github.com/Bodmer/TFT_eSPI
 */

/*
    EPD_WIDTH and EPD_HEIGHT are automatically defined here based on the library selected

    For 2 colour ePaper displays create one frame pointer in sketch:
    uint8_t* framePtr;
    
    For 3 colour ePaper displays create two frame pointers in sketch:
    uint8_t* blackFramePtr;
    uint8_t* redFramePtr;
    
    Call this function to update whole display:
    updateDisplay();
*/
// Install the ePaper library for your own display size and type
// from here:
// https://github.com/Bodmer/EPD_Libraries

 
//------------------------------------------------------------------------------------
// Define which colour values are paper and ink
//------------------------------------------------------------------------------------
#if defined (EPD2IN7B_H)
  #define COLORED     1 // EPD2IN7B is opposite to all others!
  #define UNCOLORED   0
#else
  #define COLORED     0
  #define UNCOLORED   1
#endif


//------------------------------------------------------------------------------------
// Define the width and height of the different displays
//------------------------------------------------------------------------------------
#if defined (EPD1IN54_H) || defined (EPD1IN54B_H)
  #define EPD_WIDTH       200 // Frame buffer is 5000 bytes
  #define EPD_HEIGHT      200
  #define INIT_LUT        lut_full_update

#elif defined (EPD1IN54C_H)
  #define EPD_WIDTH       152 // 2 frame buffers of 2888 bytes each
  #define EPD_HEIGHT      152
  #define INIT_LUT

#elif defined (EPD2IN7_H) || defined (EPD2IN7B_H)
  #define EPD_WIDTH       176 // Frame buffer is 5808 bytes
  #define EPD_HEIGHT      264
  #define INIT_LUT

#elif defined (EPD2IN9_H)
  #define EPD_WIDTH       128 // Frame buffer is 4736 bytes
  #define EPD_HEIGHT      296
  #define INIT_LUT        lut_full_update

#elif defined (EPD2IN9B_H)
  #define EPD_WIDTH       128 // Frame buffer is 4736 bytes
  #define EPD_HEIGHT      296
  #define INIT_LUT

#elif defined (EPD2IN13_H)
  #define EPD_WIDTH       122 // Frame buffer is 4000 bytes
  #define EPD_HEIGHT      250
  #define INIT_LUT        lut_full_update

#elif defined (EPD2IN13B_H)
  #define EPD_WIDTH       104 // 2 frame buffers of 2756 bytes each
  #define EPD_HEIGHT      212
  #define INIT_LUT

#elif defined (EPD4IN2_H) || defined (EPD4IN2B_H)
  #define EPD_WIDTH       400 // Frame buffer is 15000 bytes
  #define EPD_HEIGHT      300
  #define INIT_LUT

// ESP8266 has just enough RAM for a 2 color 7.5" display full screen buffer
// ESP32 has just enough RAM for 2 or 3 color 7.5" display
// (Without using partial screen updates)
#elif defined (EPD7IN5_H) || defined (EPD7IN5B_H)
  #define EPD_WIDTH       640  // 2 colour frame buffer is 30720 bytes
  #define EPD_HEIGHT      384  // 2 colour frame buffer is 61440 bytes
  #define INIT_LUT

#else
  # error "Selected ePaper library is not supported"

#endif


//------------------------------------------------------------------------------------
// Update display - different displays have different function names in the default
// Waveshare libraries  :-(
//------------------------------------------------------------------------------------
#if defined (EPD1IN54B_H) || defined(EPD1IN54C_H) || defined(EPD2IN13B_H) || defined(EPD2IN7B_H) || defined(EPD2IN9B_H) || defined(EPD4IN2_H)
  void updateDisplay(uint8_t* blackFrame = blackFramePtr, uint8_t* redFrame = redFramePtr)
  {
    ePaper.DisplayFrame(blackFrame, redFrame);  // Update 3 colour display
#else
  void updateDisplay(uint8_t* blackFrame = framePtr)
  {
  #if defined (EPD2IN7_H) || defined(EPD4IN2_H)
    ePaper.DisplayFrame(blackFrame);            // Update 2 color display

  #elif defined (EPD1IN54_H) || defined(EPD2IN13_H) || defined(EPD2IN9_H)
    ePaper.SetFrameMemory(blackFrame);
    ePaper.DisplayFrame();            // Update 2 color display
  #else
    # error "Selected ePaper library is not supported"
  #endif
#endif
}
