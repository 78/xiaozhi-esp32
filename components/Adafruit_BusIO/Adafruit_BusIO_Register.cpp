#include <Adafruit_BusIO_Register.h>

#if !defined(SPI_INTERFACES_COUNT) ||                                          \
    (defined(SPI_INTERFACES_COUNT) && (SPI_INTERFACES_COUNT > 0))

/*!
 *    @brief  Create a register we access over an I2C Device (which defines the
 * bus and address)
 *    @param  i2cdevice The I2CDevice to use for underlying I2C access
 *    @param  reg_addr The address pointer value for the I2C/SMBus register, can
 * be 8 or 16 bits
 *    @param  width    The width of the register data itself, defaults to 1 byte
 *    @param  byteorder The byte order of the register (used when width is > 1),
 * defaults to LSBFIRST
 *    @param  address_width The width of the register address itself, defaults
 * to 1 byte
 */
Adafruit_BusIO_Register::Adafruit_BusIO_Register(Adafruit_I2CDevice *i2cdevice,
                                                 uint16_t reg_addr,
                                                 uint8_t width,
                                                 uint8_t byteorder,
                                                 uint8_t address_width) {
  _i2cdevice = i2cdevice;
  _spidevice = nullptr;
  _addrwidth = address_width;
  _address = reg_addr;
  _byteorder = byteorder;
  _width = width;
}

/*!
 *    @brief  Create a register we access over an SPI Device (which defines the
 * bus and CS pin)
 *    @param  spidevice The SPIDevice to use for underlying SPI access
 *    @param  reg_addr The address pointer value for the SPI register, can
 * be 8 or 16 bits
 *    @param  type     The method we use to read/write data to SPI (which is not
 * as well defined as I2C)
 *    @param  width    The width of the register data itself, defaults to 1 byte
 *    @param  byteorder The byte order of the register (used when width is > 1),
 * defaults to LSBFIRST
 *    @param  address_width The width of the register address itself, defaults
 * to 1 byte
 */
Adafruit_BusIO_Register::Adafruit_BusIO_Register(Adafruit_SPIDevice *spidevice,
                                                 uint16_t reg_addr,
                                                 Adafruit_BusIO_SPIRegType type,
                                                 uint8_t width,
                                                 uint8_t byteorder,
                                                 uint8_t address_width) {
  _spidevice = spidevice;
  _spiregtype = type;
  _i2cdevice = nullptr;
  _addrwidth = address_width;
  _address = reg_addr;
  _byteorder = byteorder;
  _width = width;
}

/*!
 *    @brief  Create a register we access over an I2C or SPI Device. This is a
 * handy function because we can pass in nullptr for the unused interface,
 * allowing libraries to mass-define all the registers
 *    @param  i2cdevice The I2CDevice to use for underlying I2C access, if
 * nullptr we use SPI
 *    @param  spidevice The SPIDevice to use for underlying SPI access, if
 * nullptr we use I2C
 *    @param  reg_addr The address pointer value for the I2C/SMBus/SPI register,
 * can be 8 or 16 bits
 *    @param  type     The method we use to read/write data to SPI (which is not
 * as well defined as I2C)
 *    @param  width    The width of the register data itself, defaults to 1 byte
 *    @param  byteorder The byte order of the register (used when width is > 1),
 * defaults to LSBFIRST
 *    @param  address_width The width of the register address itself, defaults
 * to 1 byte
 */
Adafruit_BusIO_Register::Adafruit_BusIO_Register(
    Adafruit_I2CDevice *i2cdevice, Adafruit_SPIDevice *spidevice,
    Adafruit_BusIO_SPIRegType type, uint16_t reg_addr, uint8_t width,
    uint8_t byteorder, uint8_t address_width) {
  _spidevice = spidevice;
  _i2cdevice = i2cdevice;
  _spiregtype = type;
  _addrwidth = address_width;
  _address = reg_addr;
  _byteorder = byteorder;
  _width = width;
}

/*!
 * @brief Create a register we access over a GenericDevice
 * @param genericdevice Generic device to use
 * @param reg_addr Register address we will read/write
 * @param width Width of the register in bytes (1-4)
 * @param byteorder Byte order of register data (LSBFIRST or MSBFIRST)
 * @param address_width Width of the register address in bytes (1 or 2)
 */
Adafruit_BusIO_Register::Adafruit_BusIO_Register(
    Adafruit_GenericDevice *genericdevice, uint16_t reg_addr, uint8_t width,
    uint8_t byteorder, uint8_t address_width) {
  _i2cdevice = nullptr;
  _spidevice = nullptr;
  _genericdevice = genericdevice;
  _addrwidth = address_width;
  _address = reg_addr;
  _byteorder = byteorder;
  _width = width;
}

/*!
 *    @brief  Write a buffer of data to the register location
 *    @param  buffer Pointer to data to write
 *    @param  len Number of bytes to write
 *    @return True on successful write (only really useful for I2C as SPI is
 * uncheckable)
 */
bool Adafruit_BusIO_Register::write(uint8_t *buffer, uint8_t len) {
  uint8_t addrbuffer[2] = {(uint8_t)(_address & 0xFF),
                           (uint8_t)(_address >> 8)};
  if (_i2cdevice) {
    return _i2cdevice->write(buffer, len, true, addrbuffer, _addrwidth);
  }
  if (_spidevice) {
    if (_spiregtype == ADDRESSED_OPCODE_BIT0_LOW_TO_WRITE) {
      // very special case!
      // pass the special opcode address which we set as the high byte of the
      // regaddr
      addrbuffer[0] =
          (uint8_t)(_address >> 8) & ~0x01; // set bottom bit low to write
      // the 'actual' reg addr is the second byte then
      addrbuffer[1] = (uint8_t)(_address & 0xFF);
      // the address appears to be a byte longer
      return _spidevice->write(buffer, len, addrbuffer, _addrwidth + 1);
    }
    if (_spiregtype == ADDRBIT8_HIGH_TOREAD) {
      addrbuffer[0] &= ~0x80;
    }
    if (_spiregtype == ADDRBIT8_HIGH_TOWRITE) {
      addrbuffer[0] |= 0x80;
    }
    if (_spiregtype == AD8_HIGH_TOREAD_AD7_HIGH_TOINC) {
      addrbuffer[0] &= ~0x80;
      addrbuffer[0] |= 0x40;
    }
    return _spidevice->write(buffer, len, addrbuffer, _addrwidth);
  }
  if (_genericdevice) {
    return _genericdevice->writeRegister(addrbuffer, _addrwidth, buffer, len);
  }
  return false;
}

/*!
 *    @brief  Write up to 4 bytes of data to the register location
 *    @param  value Data to write
 *    @param  numbytes How many bytes from 'value' to write
 *    @return True on successful write (only really useful for I2C as SPI is
 * uncheckable)
 */
bool Adafruit_BusIO_Register::write(uint32_t value, uint8_t numbytes) {
  if (numbytes == 0) {
    numbytes = _width;
  }
  if (numbytes > 4) {
    return false;
  }

  // store a copy
  _cached = value;

  for (int i = 0; i < numbytes; i++) {
    if (_byteorder == LSBFIRST) {
      _buffer[i] = value & 0xFF;
    } else {
      _buffer[numbytes - i - 1] = value & 0xFF;
    }
    value >>= 8;
  }
  return write(_buffer, numbytes);
}

/*!
 *    @brief  Read data from the register location. This does not do any error
 * checking!
 *    @return Returns 0xFFFFFFFF on failure, value otherwise
 */
uint32_t Adafruit_BusIO_Register::read(void) {
  if (!read(_buffer, _width)) {
    return -1;
  }

  uint32_t value = 0;

  for (int i = 0; i < _width; i++) {
    value <<= 8;
    if (_byteorder == LSBFIRST) {
      value |= _buffer[_width - i - 1];
    } else {
      value |= _buffer[i];
    }
  }

  return value;
}

/*!
 *    @brief  Read cached data from last time we wrote to this register
 *    @return Returns 0xFFFFFFFF on failure, value otherwise
 */
uint32_t Adafruit_BusIO_Register::readCached(void) { return _cached; }

/*!
   @brief Read a number of bytes from a register into a buffer
   @param buffer Buffer to read data into
   @param len Number of bytes to read into the buffer
   @return true on successful read, otherwise false
*/
bool Adafruit_BusIO_Register::read(uint8_t *buffer, uint8_t len) {
  uint8_t addrbuffer[2] = {(uint8_t)(_address & 0xFF),
                           (uint8_t)(_address >> 8)};
  if (_i2cdevice) {
    return _i2cdevice->write_then_read(addrbuffer, _addrwidth, buffer, len);
  }
  if (_spidevice) {
    if (_spiregtype == ADDRESSED_OPCODE_BIT0_LOW_TO_WRITE) {
      // very special case!
      // pass the special opcode address which we set as the high byte of the
      // regaddr
      addrbuffer[0] =
          (uint8_t)(_address >> 8) | 0x01; // set bottom bit high to read
      // the 'actual' reg addr is the second byte then
      addrbuffer[1] = (uint8_t)(_address & 0xFF);
      // the address appears to be a byte longer
      return _spidevice->write_then_read(addrbuffer, _addrwidth + 1, buffer,
                                         len);
    }
    if (_spiregtype == ADDRBIT8_HIGH_TOREAD) {
      addrbuffer[0] |= 0x80;
    }
    if (_spiregtype == ADDRBIT8_HIGH_TOWRITE) {
      addrbuffer[0] &= ~0x80;
    }
    if (_spiregtype == AD8_HIGH_TOREAD_AD7_HIGH_TOINC) {
      addrbuffer[0] |= 0x80 | 0x40;
    }
    return _spidevice->write_then_read(addrbuffer, _addrwidth, buffer, len);
  }
  if (_genericdevice) {
    return _genericdevice->readRegister(addrbuffer, _addrwidth, buffer, len);
  }
  return false;
}

/*!
 *    @brief  Read 2 bytes of data from the register location
 *    @param  value Pointer to uint16_t variable to read into
 *    @return True on successful write (only really useful for I2C as SPI is
 * uncheckable)
 */
bool Adafruit_BusIO_Register::read(uint16_t *value) {
  if (!read(_buffer, 2)) {
    return false;
  }

  if (_byteorder == LSBFIRST) {
    *value = _buffer[1];
    *value <<= 8;
    *value |= _buffer[0];
  } else {
    *value = _buffer[0];
    *value <<= 8;
    *value |= _buffer[1];
  }
  return true;
}

/*!
 *    @brief  Read 1 byte of data from the register location
 *    @param  value Pointer to uint8_t variable to read into
 *    @return True on successful write (only really useful for I2C as SPI is
 * uncheckable)
 */
bool Adafruit_BusIO_Register::read(uint8_t *value) {
  if (!read(_buffer, 1)) {
    return false;
  }

  *value = _buffer[0];
  return true;
}

/*!
 *    @brief  Pretty printer for this register
 *    @param  s The Stream to print to, defaults to &Serial
 */
void Adafruit_BusIO_Register::print(Stream *s) {
  uint32_t val = read();
  s->print("0x");
  s->print(val, HEX);
}

/*!
 *    @brief  Pretty printer for this register
 *    @param  s The Stream to print to, defaults to &Serial
 */
void Adafruit_BusIO_Register::println(Stream *s) {
  print(s);
  s->println();
}

/*!
 *    @brief  Create a slice of the register that we can address without
 * touching other bits
 *    @param  reg The Adafruit_BusIO_Register which defines the bus/register
 *    @param  bits The number of bits wide we are slicing
 *    @param  shift The number of bits that our bit-slice is shifted from LSB
 */
Adafruit_BusIO_RegisterBits::Adafruit_BusIO_RegisterBits(
    Adafruit_BusIO_Register *reg, uint8_t bits, uint8_t shift) {
  _register = reg;
  _bits = bits;
  _shift = shift;
}

/*!
 *    @brief  Read 4 bytes of data from the register
 *    @return  data The 4 bytes to read
 */
uint32_t Adafruit_BusIO_RegisterBits::read(void) {
  uint32_t val = _register->read();
  val >>= _shift;
  return val & ((1 << (_bits)) - 1);
}

/*!
 *    @brief  Write 4 bytes of data to the register
 *    @param  data The 4 bytes to write
 *    @return True on successful write (only really useful for I2C as SPI is
 * uncheckable)
 */
bool Adafruit_BusIO_RegisterBits::write(uint32_t data) {
  uint32_t val = _register->read();

  // mask off the data before writing
  uint32_t mask = (1 << (_bits)) - 1;
  data &= mask;

  mask <<= _shift;
  val &= ~mask;          // remove the current data at that spot
  val |= data << _shift; // and add in the new data

  return _register->write(val, _register->width());
}

/*!
 *    @brief  The width of the register data, helpful for doing calculations
 *    @returns The data width used when initializing the register
 */
uint8_t Adafruit_BusIO_Register::width(void) { return _width; }

/*!
 *    @brief  Set the default width of data
 *    @param width the default width of data read from register
 */
void Adafruit_BusIO_Register::setWidth(uint8_t width) { _width = width; }

/*!
 *    @brief  Set register address
 *    @param address the address from register
 */
void Adafruit_BusIO_Register::setAddress(uint16_t address) {
  _address = address;
}

/*!
 *    @brief  Set the width of register address
 *    @param address_width the width for register address
 */
void Adafruit_BusIO_Register::setAddressWidth(uint16_t address_width) {
  _addrwidth = address_width;
}

#endif // SPI exists
