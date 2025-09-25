#include <Adafruit_BusIO_Register.h>
#include <Adafruit_I2CDevice.h>

#define I2C_ADDRESS 0x60
Adafruit_I2CDevice i2c_dev = Adafruit_I2CDevice(I2C_ADDRESS);

void setup() {
  while (!Serial) {
    delay(10);
  }
  Serial.begin(115200);
  Serial.println("I2C device register test");

  if (!i2c_dev.begin()) {
    Serial.print("Did not find device at 0x");
    Serial.println(i2c_dev.address(), HEX);
    while (1)
      ;
  }
  Serial.print("Device found on address 0x");
  Serial.println(i2c_dev.address(), HEX);

  Adafruit_BusIO_Register id_reg =
      Adafruit_BusIO_Register(&i2c_dev, 0x0C, 2, LSBFIRST);
  uint16_t id;
  id_reg.read(&id);
  Serial.print("ID register = 0x");
  Serial.println(id, HEX);

  Adafruit_BusIO_Register thresh_reg =
      Adafruit_BusIO_Register(&i2c_dev, 0x01, 2, LSBFIRST);
  uint16_t thresh;
  thresh_reg.read(&thresh);
  Serial.print("Initial threshold register = 0x");
  Serial.println(thresh, HEX);

  thresh_reg.write(~thresh);

  Serial.print("Post threshold register = 0x");
  Serial.println(thresh_reg.read(), HEX);
}

void loop() {}