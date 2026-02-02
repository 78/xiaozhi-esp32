 // Coded by Bodmer 10/2/18, see license in root directory.
 // This is part of the TFT_eSPI class and is associated with anti-aliased font functions

 public:

  // These are for the new anti-aliased fonts
  void     loadFont(const uint8_t array[]);
#ifdef FONT_FS_AVAILABLE
  void     loadFont(String fontName, fs::FS &ffs);
#endif
  void     loadFont(String fontName, bool flash = true);
  void     unloadFont( void );
  bool     getUnicodeIndex(uint16_t unicode, uint16_t *index);

  virtual void drawGlyph(uint16_t code);

  void     showFont(uint32_t td);

 // This is for the whole font
  typedef struct
  {
    const uint8_t* gArray;           //array start pointer
    uint16_t gCount;                 // Total number of characters
    uint16_t yAdvance;               // Line advance
    uint16_t spaceWidth;             // Width of a space character
    int16_t  ascent;                 // Height of top of 'd' above baseline, other characters may be taller
    int16_t  descent;                // Offset to bottom of 'p', other characters may have a larger descent
    uint16_t maxAscent;              // Maximum ascent found in font
    uint16_t maxDescent;             // Maximum descent found in font
  } fontMetrics;

fontMetrics gFont = { nullptr, 0, 0, 0, 0, 0, 0, 0 };

  // These are for the metrics for each individual glyph (so we don't need to seek this in file and waste time)
  uint16_t* gUnicode = NULL;  //UTF-16 code, the codes are searched so do not need to be sequential
  uint8_t*  gHeight = NULL;   //cheight
  uint8_t*  gWidth = NULL;    //cwidth
  uint8_t*  gxAdvance = NULL; //setWidth
  int16_t*  gdY = NULL;       //topExtent
  int8_t*   gdX = NULL;       //leftExtent
  uint32_t* gBitmap = NULL;   //file pointer to greyscale bitmap

  bool     fontLoaded = false; // Flags when a anti-aliased font is loaded

#ifdef FONT_FS_AVAILABLE
  fs::File fontFile;
  fs::FS   &fontFS  = SPIFFS;
  bool     spiffs   = true;
  bool     fs_font = false;    // For ESP32/8266 use smooth font file or FLASH (PROGMEM) array

#else
  bool     fontFile = true;
#endif

  private:

  void     loadMetrics(void);
  uint32_t readInt32(void);

  uint8_t* fontPtr = nullptr;

