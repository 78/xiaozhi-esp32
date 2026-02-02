// Read the ID from the TFT controller
// The sketch only supports displays with a bi-directional SPI data line (often labelled SDA)

// Bit bashes SPI so it does NOT assume hardware SPI wired up
// No other libraries are needed

// NOTE: This sketch does not work with parallel displays!

// Original author unknown
// Adapted by Bodmer 22/5/16, updated 16/9/16

// Change the pin settings to suit your hardware

// UNO etc.
//#define TFT_MOSI  11
//#define TFT_SCK 13
//#define TFT_CS 9
//#define TFT_DC  8
//#define TFT_RESET 7

//Mega
//#define TFT_MOSI  51
//#define TFT_SCK 52
//#define TFT_CS 47
//#define TFT_DC  48
//#define TFT_RESET 44

//Micro
//#define TFT_MOSI  MOSI
//#define TFT_SCK SCK
//#define TFT_CS 10
//#define TFT_DC  9
//#define TFT_RESET 8

//NodeMCU
#define TFT_MOSI  D7
#define TFT_SCK D5
#define TFT_CS D8
#define TFT_DC  D3
#define TFT_RESET D4

/* Example Serial Monitor output:

TFT driver register values:
===========================
Register 0x01: 0x00
Register 0x04: 0x548066
Register 0x09: 0x610000
Register 0x0A: 0x08
Register 0x0B: 0x00
Register 0x0C: 0x06
Register 0x0D: 0x00
Register 0x0E: 0x00
Register 0x0F: 0x00
Register 0x2E: 0x1834B4
Register 0xDA: 0x54
Register 0xDB: 0x80
Register 0xDC: 0x66
===========================

Looks like driver chip is: ILI9163 (based on datasheet ID)

*/

char *chip = "Unknown                                 ";

uint32_t readwrite8(uint8_t cmd, uint8_t bits, uint8_t dummy)
{
    uint32_t ret = 0;
    uint8_t val = cmd;
    int cnt = 8;
    digitalWrite(TFT_CS, LOW);
    digitalWrite(TFT_DC, LOW);
    pinMode(TFT_MOSI, OUTPUT);
    for (int i = 0; i < 8; i++) {   //send command
        digitalWrite(TFT_MOSI, (val & 0x80) != 0);
        digitalWrite(TFT_SCK, HIGH);
        digitalWrite(TFT_SCK, LOW);
        val <<= 1;
    }
    if (bits == 0) {
        digitalWrite(TFT_CS, HIGH);
        return 0;
    }
    pinMode(TFT_MOSI, INPUT_PULLUP);
    digitalWrite(TFT_DC, HIGH);
    for (int i = 0; i < dummy; i++) {  //any dummy clocks
        digitalWrite(TFT_SCK, HIGH);
        digitalWrite(TFT_SCK, LOW);
    }
    for (int i = 0; i < bits; i++) {  // read results
        ret <<= 1;
        if (digitalRead(TFT_MOSI)) ret |= 1;;
        digitalWrite(TFT_SCK, HIGH);
        digitalWrite(TFT_SCK, LOW);
    }
    digitalWrite(TFT_CS, HIGH);
    return ret;
}

void showreg(uint8_t reg, uint8_t bits, uint8_t dummy)
{
    uint32_t val;
    val = readwrite8(reg, bits, dummy);

    Serial.print("Register 0x");
    if (reg < 0x10) Serial.print("0");
    Serial.print(reg , HEX);
    Serial.print(": 0x");
    if (val < 0x10) Serial.print("0");
    Serial.println(val, HEX);
}

void setup() {
    // put your setup code here, to run once:
    uint32_t ID = 0;
    Serial.begin(115200);

    delay(4000); // Delay to allow USB to connect to a Leonardo etc.
    
    Serial.println("TFT driver register values:");
    Serial.println("===========================");
    digitalWrite(TFT_CS, HIGH);
    //    digitalWrite(TFT_SCK, HIGH);
    pinMode(TFT_CS, OUTPUT);
    pinMode(TFT_SCK, OUTPUT);
    pinMode(TFT_MOSI, OUTPUT);
    pinMode(MISO, INPUT_PULLUP);
    pinMode(TFT_DC, OUTPUT);
    pinMode(TFT_RESET, OUTPUT);
    digitalWrite(TFT_RESET, HIGH);
    digitalWrite(TFT_RESET, LOW);   //Hardware Reset
    delay(50);
    digitalWrite(TFT_RESET, HIGH);
    showreg(0x01, 0, 0);            //Software Reset
    delay(100);
    ID = readwrite8(0x04, 24, 1);

    if ((ID & 0xFF8000) == 0x5C8000uL) chip = "ST7735 (based on datasheet ID)";
    if (ID == 0x7C89F0uL) chip =              "ST7735S (empirical value)";
    if (ID == 0x548066uL) chip =              "ILI9163C (based on datasheet ID)";
    if (ID == 0x5C0000uL) chip =              "S6D02A1 (based on datasheet ID)";
 
    showreg(0x04, 24, 1);   //RDDID
    showreg(0x09, 32, 1);   //RDSTATUS
    showreg(0x0A, 8, 0);
    showreg(0x0B, 8, 0);
    showreg(0x0C, 8, 0);
    showreg(0x0D, 8, 0);
    showreg(0x0E, 8, 0);
    showreg(0x0F, 8, 0);
    showreg(0x2E, 24, 8);   //readGRAM
    showreg(0x3F, 32, 1);   //readGRAM
    showreg(0xDA, 8, 0);
    showreg(0xDB, 8, 0);
    showreg(0xDC, 8, 0);

    Serial.println("===========================");
    Serial.println();
    Serial.print("Looks like driver chip is: " );Serial.println(chip);
}

void loop() {

}



