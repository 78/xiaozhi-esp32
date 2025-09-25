#include <Adafruit_BusIO_Register.h>
#include <Adafruit_SPIDevice.h>

#define SPIDEVICE_CS 10
Adafruit_SPIDevice spi_dev = Adafruit_SPIDevice(SPIDEVICE_CS);

void setup() {
  while (!Serial) {
    delay(10);
  }
  Serial.begin(115200);
  Serial.println("SPI device register test");

  if (!spi_dev.begin()) {
    Serial.println("Could not initialize SPI device");
    while (1)
      ;
  }

  Adafruit_BusIO_Register id_reg =
      Adafruit_BusIO_Register(&spi_dev, 0x0F, ADDRBIT8_HIGH_TOREAD);
  uint8_t id = 0;
  id_reg.read(&id);
  Serial.print("ID register = 0x");
  Serial.println(id, HEX);

  Adafruit_BusIO_Register thresh_reg = Adafruit_BusIO_Register(
      &spi_dev, 0x0C, ADDRBIT8_HIGH_TOREAD, 2, LSBFIRST);
  uint16_t thresh = 0;
  thresh_reg.read(&thresh);
  Serial.print("Initial threshold register = 0x");
  Serial.println(thresh, HEX);

  thresh_reg.write(~thresh);

  Serial.print("Post threshold register = 0x");
  Serial.println(thresh_reg.read(), HEX);
}

void loop() {}
