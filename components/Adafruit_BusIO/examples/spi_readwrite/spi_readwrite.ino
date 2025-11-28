#include <Adafruit_SPIDevice.h>

#define SPIDEVICE_CS 10
Adafruit_SPIDevice spi_dev = Adafruit_SPIDevice(SPIDEVICE_CS);

void setup() {
  while (!Serial) {
    delay(10);
  }
  Serial.begin(115200);
  Serial.println("SPI device read and write test");

  if (!spi_dev.begin()) {
    Serial.println("Could not initialize SPI device");
    while (1)
      ;
  }

  uint8_t buffer[32];

  // Try to read 32 bytes
  spi_dev.read(buffer, 32);
  Serial.print("Read: ");
  for (uint8_t i = 0; i < 32; i++) {
    Serial.print("0x");
    Serial.print(buffer[i], HEX);
    Serial.print(", ");
  }
  Serial.println();

  // read a register by writing first, then reading
  buffer[0] = 0x8F; // we'll reuse the same buffer
  spi_dev.write_then_read(buffer, 1, buffer, 2, false);
  Serial.print("Write then Read: ");
  for (uint8_t i = 0; i < 2; i++) {
    Serial.print("0x");
    Serial.print(buffer[i], HEX);
    Serial.print(", ");
  }
  Serial.println();
}

void loop() {}
