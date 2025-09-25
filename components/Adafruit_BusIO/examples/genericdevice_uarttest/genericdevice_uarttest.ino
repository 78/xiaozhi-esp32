/*
   Abstracted transport for reading and writing data from a UART-based
   device such as a TMC2209

   Written with help by Claude!
  https://claude.ai/chat/335f50b1-3dd8-435e-9139-57ec7ca26a3c (at this time
  chats are not shareable :(
*/

#include "Adafruit_GenericDevice.h"

/**
 * Basic UART device class that demonstrates using GenericDevice with a Stream
 * interface. This example shows how to wrap a Stream (like HardwareSerial or
 * SoftwareSerial) with read/write callbacks that can be used by BusIO's
 * register functions.
 */
class UARTDevice {
public:
  UARTDevice(Stream *serial) : _serial(serial) {}

  // Static callback for writing data to UART
  // Called by GenericDevice when data needs to be sent
  static bool uart_write(void *thiz, const uint8_t *buffer, size_t len) {
    UARTDevice *dev = (UARTDevice *)thiz;
    dev->_serial->write(buffer, len);
    return true;
  }

  // Static callback for reading data from UART
  // Includes timeout and will return false if not enough data available
  static bool uart_read(void *thiz, uint8_t *buffer, size_t len) {
    UARTDevice *dev = (UARTDevice *)thiz;
    uint16_t timeout = 100;
    while (dev->_serial->available() < len && timeout--) {
      delay(1);
    }
    if (timeout == 0) {
      return false;
    }
    for (size_t i = 0; i < len; i++) {
      buffer[i] = dev->_serial->read();
    }
    return true;
  }

  // Create a GenericDevice instance using our callbacks
  Adafruit_GenericDevice *createDevice() {
    return new Adafruit_GenericDevice(this, uart_read, uart_write);
  }

private:
  Stream *_serial; // Underlying Stream instance (HardwareSerial, etc)
};

void setup() {
  Serial.begin(115200);
  while (!Serial)
    ;
  delay(100);

  Serial.println("Generic Device test!");

  // Initialize UART for device communication
  Serial1.begin(115200);

  // Create UART wrapper and BusIO device
  UARTDevice uart(&Serial1);
  Adafruit_GenericDevice *device = uart.createDevice();
  device->begin();

  // Test write/read cycle
  uint8_t write_buf[4] = {0x5, 0x0, 0x0, 0x48};
  uint8_t read_buf[8];

  Serial.println("Writing data...");
  if (!device->write(write_buf, 4)) {
    Serial.println("Write failed!");
    return;
  }

  Serial.println("Reading response...");
  if (!device->read(read_buf, 8)) {
    Serial.println("Read failed!");
    return;
  }

  // Print response bytes
  Serial.print("Got response: ");
  for (int i = 0; i < 8; i++) {
    Serial.print("0x");
    Serial.print(read_buf[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
}

void loop() { delay(1000); }