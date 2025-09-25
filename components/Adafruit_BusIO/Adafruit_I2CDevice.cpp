#include "Adafruit_I2CDevice.h"

// #define DEBUG_SERIAL Serial

/*!
 *    @brief  Create an I2C device at a given address
 *    @param  addr The 7-bit I2C address for the device
 *    @param  theWire The I2C bus to use, defaults to &Wire
 */
Adafruit_I2CDevice::Adafruit_I2CDevice(uint8_t addr, TwoWire *theWire) {
  _addr = addr;
  _wire = theWire;
  _begun = false;
#ifdef ARDUINO_ARCH_SAMD
  _maxBufferSize = 250; // as defined in Wire.h's RingBuffer
#elif defined(ESP32)
  _maxBufferSize = I2C_BUFFER_LENGTH;
#else
  _maxBufferSize = 32;
#endif
}

/*!
 *    @brief  Initializes and does basic address detection
 *    @param  addr_detect Whether we should attempt to detect the I2C address
 * with a scan. 99% of sensors/devices don't mind, but once in a while they
 * don't respond well to a scan!
 *    @return True if I2C initialized and a device with the addr found
 */
bool Adafruit_I2CDevice::begin(bool addr_detect) {
  _wire->begin();
  _begun = true;

  if (addr_detect) {
    return detected();
  }
  return true;
}

/*!
 *    @brief  De-initialize device, turn off the Wire interface
 */
void Adafruit_I2CDevice::end(void) {
  // Not all port implement Wire::end(), such as
  // - ESP8266
  // - AVR core without WIRE_HAS_END
  // - ESP32: end() is implemented since 2.0.1 which is latest at the moment.
  // Temporarily disable for now to give time for user to update.
#if !(defined(ESP8266) ||                                                      \
      (defined(ARDUINO_ARCH_AVR) && !defined(WIRE_HAS_END)) ||                 \
      defined(ARDUINO_ARCH_ESP32))
  _wire->end();
  _begun = false;
#endif
}

/*!
 *    @brief  Scans I2C for the address - note will give a false-positive
 *    if there's no pullups on I2C
 *    @return True if I2C initialized and a device with the addr found
 */
bool Adafruit_I2CDevice::detected(void) {
  // Init I2C if not done yet
  if (!_begun && !begin()) {
    return false;
  }

  // A basic scanner, see if it ACK's
  _wire->beginTransmission(_addr);
#ifdef DEBUG_SERIAL
  DEBUG_SERIAL.print(F("Address 0x"));
  DEBUG_SERIAL.print(_addr, HEX);
#endif
#ifdef ARDUINO_ARCH_MBED
  _wire->write(0); // forces a write request instead of a read
#endif
  if (_wire->endTransmission() == 0) {
#ifdef DEBUG_SERIAL
    DEBUG_SERIAL.println(F(" Detected"));
#endif
    return true;
  }
#ifdef DEBUG_SERIAL
  DEBUG_SERIAL.println(F(" Not detected"));
#endif
  return false;
}

/*!
 *    @brief  Write a buffer or two to the I2C device. Cannot be more than
 * maxBufferSize() bytes.
 *    @param  buffer Pointer to buffer of data to write. This is const to
 *            ensure the content of this buffer doesn't change.
 *    @param  len Number of bytes from buffer to write
 *    @param  prefix_buffer Pointer to optional array of data to write before
 * buffer. Cannot be more than maxBufferSize() bytes. This is const to
 *            ensure the content of this buffer doesn't change.
 *    @param  prefix_len Number of bytes from prefix buffer to write
 *    @param  stop Whether to send an I2C STOP signal on write
 *    @return True if write was successful, otherwise false.
 */
bool Adafruit_I2CDevice::write(const uint8_t *buffer, size_t len, bool stop,
                               const uint8_t *prefix_buffer,
                               size_t prefix_len) {
  if ((len + prefix_len) > maxBufferSize()) {
    // currently not guaranteed to work if more than 32 bytes!
    // we will need to find out if some platforms have larger
    // I2C buffer sizes :/
#ifdef DEBUG_SERIAL
    DEBUG_SERIAL.println(F("\tI2CDevice could not write such a large buffer"));
#endif
    return false;
  }

  _wire->beginTransmission(_addr);

  // Write the prefix data (usually an address)
  if ((prefix_len != 0) && (prefix_buffer != nullptr)) {
    if (_wire->write(prefix_buffer, prefix_len) != prefix_len) {
#ifdef DEBUG_SERIAL
      DEBUG_SERIAL.println(F("\tI2CDevice failed to write"));
#endif
      return false;
    }
  }

  // Write the data itself
  if (_wire->write(buffer, len) != len) {
#ifdef DEBUG_SERIAL
    DEBUG_SERIAL.println(F("\tI2CDevice failed to write"));
#endif
    return false;
  }

#ifdef DEBUG_SERIAL

  DEBUG_SERIAL.print(F("\tI2CWRITE @ 0x"));
  DEBUG_SERIAL.print(_addr, HEX);
  DEBUG_SERIAL.print(F(" :: "));
  if ((prefix_len != 0) && (prefix_buffer != nullptr)) {
    for (uint16_t i = 0; i < prefix_len; i++) {
      DEBUG_SERIAL.print(F("0x"));
      DEBUG_SERIAL.print(prefix_buffer[i], HEX);
      DEBUG_SERIAL.print(F(", "));
    }
  }
  for (uint16_t i = 0; i < len; i++) {
    DEBUG_SERIAL.print(F("0x"));
    DEBUG_SERIAL.print(buffer[i], HEX);
    DEBUG_SERIAL.print(F(", "));
    if (i % 32 == 31) {
      DEBUG_SERIAL.println();
    }
  }

  if (stop) {
    DEBUG_SERIAL.print("\tSTOP");
  }
#endif

  if (_wire->endTransmission(stop) == 0) {
#ifdef DEBUG_SERIAL
    DEBUG_SERIAL.println();
    // DEBUG_SERIAL.println("Sent!");
#endif
    return true;
  } else {
#ifdef DEBUG_SERIAL
    DEBUG_SERIAL.println("\tFailed to send!");
#endif
    return false;
  }
}

/*!
 *    @brief  Read from I2C into a buffer from the I2C device.
 *    Cannot be more than maxBufferSize() bytes.
 *    @param  buffer Pointer to buffer of data to read into
 *    @param  len Number of bytes from buffer to read.
 *    @param  stop Whether to send an I2C STOP signal on read
 *    @return True if read was successful, otherwise false.
 */
bool Adafruit_I2CDevice::read(uint8_t *buffer, size_t len, bool stop) {
  size_t pos = 0;
  while (pos < len) {
    size_t read_len =
        ((len - pos) > maxBufferSize()) ? maxBufferSize() : (len - pos);
    bool read_stop = (pos < (len - read_len)) ? false : stop;
    if (!_read(buffer + pos, read_len, read_stop))
      return false;
    pos += read_len;
  }
  return true;
}

bool Adafruit_I2CDevice::_read(uint8_t *buffer, size_t len, bool stop) {
#if defined(TinyWireM_h)
  size_t recv = _wire->requestFrom((uint8_t)_addr, (uint8_t)len);
#elif defined(ARDUINO_ARCH_MEGAAVR)
  size_t recv = _wire->requestFrom(_addr, len, stop);
#else
  size_t recv = _wire->requestFrom((uint8_t)_addr, (uint8_t)len, (uint8_t)stop);
#endif

  if (recv != len) {
    // Not enough data available to fulfill our obligation!
#ifdef DEBUG_SERIAL
    DEBUG_SERIAL.print(F("\tI2CDevice did not receive enough data: "));
    DEBUG_SERIAL.println(recv);
#endif
    return false;
  }

  for (uint16_t i = 0; i < len; i++) {
    buffer[i] = _wire->read();
  }

#ifdef DEBUG_SERIAL
  DEBUG_SERIAL.print(F("\tI2CREAD  @ 0x"));
  DEBUG_SERIAL.print(_addr, HEX);
  DEBUG_SERIAL.print(F(" :: "));
  for (uint16_t i = 0; i < len; i++) {
    DEBUG_SERIAL.print(F("0x"));
    DEBUG_SERIAL.print(buffer[i], HEX);
    DEBUG_SERIAL.print(F(", "));
    if (len % 32 == 31) {
      DEBUG_SERIAL.println();
    }
  }
  DEBUG_SERIAL.println();
#endif

  return true;
}

/*!
 *    @brief  Write some data, then read some data from I2C into another buffer.
 *    Cannot be more than maxBufferSize() bytes. The buffers can point to
 *    same/overlapping locations.
 *    @param  write_buffer Pointer to buffer of data to write from
 *    @param  write_len Number of bytes from buffer to write.
 *    @param  read_buffer Pointer to buffer of data to read into.
 *    @param  read_len Number of bytes from buffer to read.
 *    @param  stop Whether to send an I2C STOP signal between the write and read
 *    @return True if write & read was successful, otherwise false.
 */
bool Adafruit_I2CDevice::write_then_read(const uint8_t *write_buffer,
                                         size_t write_len, uint8_t *read_buffer,
                                         size_t read_len, bool stop) {
  if (!write(write_buffer, write_len, stop)) {
    return false;
  }

  return read(read_buffer, read_len);
}

/*!
 *    @brief  Returns the 7-bit address of this device
 *    @return The 7-bit address of this device
 */
uint8_t Adafruit_I2CDevice::address(void) { return _addr; }

/*!
 *    @brief  Change the I2C clock speed to desired (relies on
 *    underlying Wire support!
 *    @param desiredclk The desired I2C SCL frequency
 *    @return True if this platform supports changing I2C speed.
 *    Not necessarily that the speed was achieved!
 */
bool Adafruit_I2CDevice::setSpeed(uint32_t desiredclk) {
#if defined(__AVR_ATmega328__) ||                                              \
    defined(__AVR_ATmega328P__) // fix arduino core set clock
  // calculate TWBR correctly

  if ((F_CPU / 18) < desiredclk) {
#ifdef DEBUG_SERIAL
    Serial.println(F("I2C.setSpeed too high."));
#endif
    return false;
  }
  uint32_t atwbr = ((F_CPU / desiredclk) - 16) / 2;
  if (atwbr > 16320) {
#ifdef DEBUG_SERIAL
    Serial.println(F("I2C.setSpeed too low."));
#endif
    return false;
  }

  if (atwbr <= 255) {
    atwbr /= 1;
    TWSR = 0x0;
  } else if (atwbr <= 1020) {
    atwbr /= 4;
    TWSR = 0x1;
  } else if (atwbr <= 4080) {
    atwbr /= 16;
    TWSR = 0x2;
  } else { //  if (atwbr <= 16320)
    atwbr /= 64;
    TWSR = 0x3;
  }
  TWBR = atwbr;

#ifdef DEBUG_SERIAL
  Serial.print(F("TWSR prescaler = "));
  Serial.println(pow(4, TWSR));
  Serial.print(F("TWBR = "));
  Serial.println(atwbr);
#endif
  return true;
#elif (ARDUINO >= 157) && !defined(ARDUINO_STM32_FEATHER) &&                   \
    !defined(TinyWireM_h)
  _wire->setClock(desiredclk);
  return true;

#else
  (void)desiredclk;
  return false;
#endif
}
