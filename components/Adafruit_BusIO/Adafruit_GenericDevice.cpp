/*
   Written with help by Claude!
  https://claude.ai/chat/335f50b1-3dd8-435e-9139-57ec7ca26a3c (at this time
  chats are not shareable :(
*/

#include "Adafruit_GenericDevice.h"

/*!
 * @brief Create a Generic device with the provided read/write functions
 * @param obj Pointer to object instance
 * @param read_func Function pointer for reading raw data
 * @param write_func Function pointer for writing raw data
 * @param readreg_func Function pointer for reading registers (optional)
 * @param writereg_func Function pointer for writing registers (optional) */
Adafruit_GenericDevice::Adafruit_GenericDevice(
    void *obj, busio_genericdevice_read_t read_func,
    busio_genericdevice_write_t write_func,
    busio_genericdevice_readreg_t readreg_func,
    busio_genericdevice_writereg_t writereg_func) {
  _obj = obj;
  _read_func = read_func;
  _write_func = write_func;
  _readreg_func = readreg_func;
  _writereg_func = writereg_func;
  _begun = false;
}

/*! @brief Simple begin function (doesn't do much at this time)
    @return true always
*/
bool Adafruit_GenericDevice::begin(void) {
  _begun = true;
  return true;
}

/*!
@brief Marks the GenericDevice as no longer in use.
@note: Since this is a GenericDevice, if you are using this with a Serial
object, this does NOT disable serial communication or release the RX/TX pins.
That must be done manually by calling Serial.end().
*/
void Adafruit_GenericDevice::end(void) { _begun = false; }

/*! @brief Write a buffer of data
   @param buffer Pointer to buffer of data to write
   @param len Number of bytes to write
   @return true if write was successful, otherwise false */
bool Adafruit_GenericDevice::write(const uint8_t *buffer, size_t len) {
  if (!_begun)
    return false;
  return _write_func(_obj, buffer, len);
}

/*! @brief Read data into a buffer
   @param buffer Pointer to buffer to read data into
   @param len Number of bytes to read
   @return true if read was successful, otherwise false */
bool Adafruit_GenericDevice::read(uint8_t *buffer, size_t len) {
  if (!_begun)
    return false;
  return _read_func(_obj, buffer, len);
}

/*! @brief Read from a register location
   @param addr_buf Buffer containing register address
   @param addrsiz Size of register address in bytes
   @param buf Buffer to store read data
   @param bufsiz Size of data to read in bytes
   @return true if read was successful, otherwise false */
bool Adafruit_GenericDevice::readRegister(uint8_t *addr_buf, uint8_t addrsiz,
                                          uint8_t *buf, uint16_t bufsiz) {
  if (!_begun || !_readreg_func)
    return false;
  return _readreg_func(_obj, addr_buf, addrsiz, buf, bufsiz);
}

/*! @brief Write to a register location
   @param addr_buf Buffer containing register address
   @param addrsiz Size of register address in bytes
   @param buf Buffer containing data to write
   @param bufsiz Size of data to write in bytes
   @return true if write was successful, otherwise false */
bool Adafruit_GenericDevice::writeRegister(uint8_t *addr_buf, uint8_t addrsiz,
                                           const uint8_t *buf,
                                           uint16_t bufsiz) {
  if (!_begun || !_writereg_func)
    return false;
  return _writereg_func(_obj, addr_buf, addrsiz, buf, bufsiz);
}
