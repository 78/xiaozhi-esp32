#include <Adafruit_I2CDevice.h>

#define I2C_ADDRESS 0x60
Adafruit_I2CDevice i2c_dev = Adafruit_I2CDevice(I2C_ADDRESS);

void setup() {
  while (!Serial) {
    delay(10);
  }
  Serial.begin(115200);
  Serial.println("I2C device read and write test");

  if (!i2c_dev.begin()) {
    Serial.print("Did not find device at 0x");
    Serial.println(i2c_dev.address(), HEX);
    while (1)
      ;
  }
  Serial.print("Device found on address 0x");
  Serial.println(i2c_dev.address(), HEX);

  uint8_t buffer[32];
  // Try to read 32 bytes
  i2c_dev.read(buffer, 32);
  Serial.print("Read: ");
  for (uint8_t i = 0; i < 32; i++) {
    Serial.print("0x");
    Serial.print(buffer[i], HEX);
    Serial.print(", ");
  }
  Serial.println();

  // read a register by writing first, then reading
  buffer[0] = 0x0C; // we'll reuse the same buffer
  i2c_dev.write_then_read(buffer, 1, buffer, 2, false);
  Serial.print("Write then Read: ");
  for (uint8_t i = 0; i < 2; i++) {
    Serial.print("0x");
    Serial.print(buffer[i], HEX);
    Serial.print(", ");
  }
  Serial.println();
}

void loop() {}
