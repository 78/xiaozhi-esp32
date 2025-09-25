/***************************************************

  This is an example for how to use Adafruit_BusIO_RegisterBits from
 Adafruit_BusIO library.

  Designed specifically to work with the Adafruit RTD Sensor
  ----> https://www.adafruit.com/products/3328
  uisng a MAX31865 RTD-to-Digital Converter
  ----> https://datasheets.maximintegrated.com/en/ds/MAX31865.pdf

  This sensor uses SPI to communicate, 4 pins are required to
  interface.
  A fifth pin helps to detect when a new conversion is ready.

  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Example written (2020/3) by Andreas Hardtung/AnHard.
  BSD license, all text above must be included in any redistribution
 ****************************************************/

#include <Adafruit_BusIO_Register.h>
#include <Adafruit_SPIDevice.h>

#define MAX31865_SPI_SPEED (5000000)
#define MAX31865_SPI_BITORDER (SPI_BITORDER_MSBFIRST)
#define MAX31865_SPI_MODE (SPI_MODE1)

#define MAX31865_SPI_CS (10)
#define MAX31865_READY_PIN (2)

Adafruit_SPIDevice spi_dev = Adafruit_SPIDevice(
    MAX31865_SPI_CS, MAX31865_SPI_SPEED, MAX31865_SPI_BITORDER,
    MAX31865_SPI_MODE, &SPI); // Hardware SPI
// Adafruit_SPIDevice spi_dev = Adafruit_SPIDevice( MAX31865_SPI_CS, 13, 12, 11,
// MAX31865_SPI_SPEED, MAX31865_SPI_BITORDER, MAX31865_SPI_MODE); // Software
// SPI

// MAX31865 chip related
// *********************************************************************************************
Adafruit_BusIO_Register config_reg =
    Adafruit_BusIO_Register(&spi_dev, 0x00, ADDRBIT8_HIGH_TOWRITE, 1, MSBFIRST);
Adafruit_BusIO_RegisterBits bias_bit =
    Adafruit_BusIO_RegisterBits(&config_reg, 1, 7);
Adafruit_BusIO_RegisterBits auto_bit =
    Adafruit_BusIO_RegisterBits(&config_reg, 1, 6);
Adafruit_BusIO_RegisterBits oneS_bit =
    Adafruit_BusIO_RegisterBits(&config_reg, 1, 5);
Adafruit_BusIO_RegisterBits wire_bit =
    Adafruit_BusIO_RegisterBits(&config_reg, 1, 4);
Adafruit_BusIO_RegisterBits faultT_bits =
    Adafruit_BusIO_RegisterBits(&config_reg, 2, 2);
Adafruit_BusIO_RegisterBits faultR_bit =
    Adafruit_BusIO_RegisterBits(&config_reg, 1, 1);
Adafruit_BusIO_RegisterBits fi50hz_bit =
    Adafruit_BusIO_RegisterBits(&config_reg, 1, 0);

Adafruit_BusIO_Register rRatio_reg =
    Adafruit_BusIO_Register(&spi_dev, 0x01, ADDRBIT8_HIGH_TOWRITE, 2, MSBFIRST);
Adafruit_BusIO_RegisterBits rRatio_bits =
    Adafruit_BusIO_RegisterBits(&rRatio_reg, 15, 1);
Adafruit_BusIO_RegisterBits fault_bit =
    Adafruit_BusIO_RegisterBits(&rRatio_reg, 1, 0);

Adafruit_BusIO_Register maxRratio_reg =
    Adafruit_BusIO_Register(&spi_dev, 0x03, ADDRBIT8_HIGH_TOWRITE, 2, MSBFIRST);
Adafruit_BusIO_RegisterBits maxRratio_bits =
    Adafruit_BusIO_RegisterBits(&maxRratio_reg, 15, 1);

Adafruit_BusIO_Register minRratio_reg =
    Adafruit_BusIO_Register(&spi_dev, 0x05, ADDRBIT8_HIGH_TOWRITE, 2, MSBFIRST);
Adafruit_BusIO_RegisterBits minRratio_bits =
    Adafruit_BusIO_RegisterBits(&minRratio_reg, 15, 1);

Adafruit_BusIO_Register fault_reg =
    Adafruit_BusIO_Register(&spi_dev, 0x07, ADDRBIT8_HIGH_TOWRITE, 1, MSBFIRST);
Adafruit_BusIO_RegisterBits range_high_fault_bit =
    Adafruit_BusIO_RegisterBits(&fault_reg, 1, 7);
Adafruit_BusIO_RegisterBits range_low_fault_bit =
    Adafruit_BusIO_RegisterBits(&fault_reg, 1, 6);
Adafruit_BusIO_RegisterBits refin_high_fault_bit =
    Adafruit_BusIO_RegisterBits(&fault_reg, 1, 5);
Adafruit_BusIO_RegisterBits refin_low_fault_bit =
    Adafruit_BusIO_RegisterBits(&fault_reg, 1, 4);
Adafruit_BusIO_RegisterBits rtdin_low_fault_bit =
    Adafruit_BusIO_RegisterBits(&fault_reg, 1, 3);
Adafruit_BusIO_RegisterBits voltage_fault_bit =
    Adafruit_BusIO_RegisterBits(&fault_reg, 1, 2);

// Print the details of the configuration register.
void printConfig(void) {
  Serial.print("BIAS: ");
  if (bias_bit.read())
    Serial.print("ON");
  else
    Serial.print("OFF");
  Serial.print(", AUTO: ");
  if (auto_bit.read())
    Serial.print("ON");
  else
    Serial.print("OFF");
  Serial.print(", ONES: ");
  if (oneS_bit.read())
    Serial.print("ON");
  else
    Serial.print("OFF");
  Serial.print(", WIRE: ");
  if (wire_bit.read())
    Serial.print("3");
  else
    Serial.print("2/4");
  Serial.print(", FAULTCLEAR: ");
  if (faultR_bit.read())
    Serial.print("ON");
  else
    Serial.print("OFF");
  Serial.print(", ");
  if (fi50hz_bit.read())
    Serial.print("50HZ");
  else
    Serial.print("60HZ");
  Serial.println();
}

// Check and print faults. Then clear them.
void checkFaults(void) {
  if (fault_bit.read()) {
    Serial.print("MAX: ");
    Serial.println(maxRratio_bits.read());
    Serial.print("VAL: ");
    Serial.println(rRatio_bits.read());
    Serial.print("MIN: ");
    Serial.println(minRratio_bits.read());

    if (range_high_fault_bit.read())
      Serial.println("Range high fault");
    if (range_low_fault_bit.read())
      Serial.println("Range low fault");
    if (refin_high_fault_bit.read())
      Serial.println("REFIN high fault");
    if (refin_low_fault_bit.read())
      Serial.println("REFIN low fault");
    if (rtdin_low_fault_bit.read())
      Serial.println("RTDIN low fault");
    if (voltage_fault_bit.read())
      Serial.println("Voltage fault");

    faultR_bit.write(1); // clear fault
  }
}

void setup() {
#if (MAX31865_1_READY_PIN != -1)
  pinMode(MAX31865_READY_PIN, INPUT_PULLUP);
#endif

  while (!Serial) {
    delay(10);
  }
  Serial.begin(115200);
  Serial.println("SPI Adafruit_BusIO_RegisterBits test on MAX31865");

  if (!spi_dev.begin()) {
    Serial.println("Could not initialize SPI device");
    while (1)
      ;
  }

  // Set up for automode 50Hz. We don't care about selfheating. We want the
  // highest possible sampling rate.
  auto_bit.write(0);   // Don't switch filtermode while auto_mode is on.
  fi50hz_bit.write(1); // Set filter to 50Hz mode.
  faultR_bit.write(1); // Clear faults.
  bias_bit.write(1); // In automode we want to have the bias current always on.
  delay(5);          // Wait until bias current settles down.
                     // 10.5 time constants of the input RC network is required.
  // 10ms worst case for 10kω reference resistor and a 0.1µF capacitor
  // across the RTD inputs. Adafruit Module has 0.1µF and only
  // 430/4300ω So here 0.43/4.3ms
  auto_bit.write(
      1); // Now we can set automode. Automatically starting first conversion.

// Test the READY_PIN
#if (defined(MAX31865_READY_PIN) && (MAX31865_READY_PIN != -1))
  int i = 0;
  while (digitalRead(MAX31865_READY_PIN) && i++ <= 100) {
    delay(1);
  }
  if (i >= 100) {
    Serial.print("ERROR: Max31865 Pin detection does not work. PIN:");
    Serial.println(MAX31865_READY_PIN);
  }
#else
  delay(100);
#endif

  // Set ratio range.
  // Setting the temperatures would need some more calculation - not related to
  // Adafruit_BusIO_RegisterBits.
  uint16_t ratio = rRatio_bits.read();
  maxRratio_bits.write((ratio < 0x8fffu - 1000u) ? ratio + 1000u : 0x8fffu);
  minRratio_bits.write((ratio > 1000u) ? ratio - 1000u : 0u);

  printConfig();
  checkFaults();
}

void loop() {
#if (defined(MAX31865_READY_PIN) && (MAX31865_1_READY_PIN != -1))
  // Is conversion ready?
  if (!digitalRead(MAX31865_READY_PIN))
#else
  // Warant conversion is ready.
  delay(21); // 21ms for 50Hz-mode. 19ms in 60Hz-mode.
#endif
  {
    // Read ratio, calculate temperature, scale, filter and print.
    Serial.println(rRatio2C(rRatio_bits.read()) * 100.0f,
                   0); // Temperature scaled by 100
    // Check, print, clear faults.
    checkFaults();
  }

  // Do something else.
  // delay(15000);
}

// Module/Sensor related. Here Adafruit PT100 module with a 2_Wire PT100 Class C
// *****************************
float rRatio2C(uint16_t ratio) {
  // A simple linear conversion.
  const float R0 = 100.0f;
  const float Rref = 430.0f;
  const float alphaPT = 0.003850f;
  const float ADCmax = (1u << 15) - 1.0f;
  const float rscale = Rref / ADCmax;
  // Measured temperature in boiling water 101.08°C with factor a = 1 and b = 0.
  // Rref and MAX at about 22±2°C. Measured temperature in ice/water bath 0.76°C
  // with factor a = 1 and b = 0. Rref and MAX at about 22±2°C.
  // const float a = 1.0f / (alphaPT * R0);
  const float a = (100.0f / 101.08f) / (alphaPT * R0);
  // const float b = 0.0f;  // 101.08
  const float b = -0.76f; // 100.32 > 101.08

  return filterRing(((ratio * rscale) - R0) * a + b);
}

// General purpose
// *********************************************************************************************
#define RINGLENGTH 250
float filterRing(float newVal) {
  static float ring[RINGLENGTH] = {0.0};
  static uint8_t ringIndex = 0;
  static bool ringFull = false;

  if (ringIndex == RINGLENGTH) {
    ringFull = true;
    ringIndex = 0;
  }
  ring[ringIndex] = newVal;
  uint8_t loopEnd = (ringFull) ? RINGLENGTH : ringIndex + 1;
  float ringSum = 0.0f;
  for (uint8_t i = 0; i < loopEnd; i++)
    ringSum += ring[i];
  ringIndex++;
  return ringSum / loopEnd;
}
