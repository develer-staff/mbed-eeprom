/***********************************************************
Author: Bernard Borredon
Version: 1.3
  - Correct write(uint32_t address, int8_t data[], uint32_t length) for eeprom >= T24C32.
    Tested with 24C02, 24C08, 24C16, 24C64, 24C256, 24C512, 24C1025 on LPC1768 (mbed online and µVision V5.16a).
  - Correct main test.

Date : 12 decembre 2013
Version: 1.2
  - Update api documentation

Date: 11 december 2013
Version: 1.1
  - Change address parameter size form uint16_t to uint32_t (error for eeprom > 24C256).
  - Change size parameter size from uint16_t to uint32_t (error for eeprom > 24C256).
    - Correct a bug in function write(uint32_t address, int8_t data[], uint32_t length) :
      last step must be done only if it remain datas to send.
    - Add function getName.
    - Add function clear.
    - Initialize _name array.

Date: 27 december 2011
Version: 1.0

************************************************************/
#include "eeprom.h"

#define BIT_SET(x, n) (x = x | (0x01 << n))
#define BIT_TEST(x, n) (x & (0x01 << n))
#define BIT_CLEAR(x, n) (x = x & ~(0x01 << n))

const char *const EEPROM::_name[] = {"24C01", "24C02", "24C04", "24C08", "24C16", "24C32",
                                     "24C64", "24C128", "24C256", "24C512", "24C1024", "24C1025", "M24M02"};

/**
 * EEPROM(PinName sda, PinName scl, uint8_t address, TypeEeprom type) : _i2c(sda, scl)
 *
 * Constructor, initialize the eeprom on i2c interface.
 * @param sda sda i2c pin (PinName)
 * @param scl scl i2c pin (PinName)
 * @param address eeprom address, according to eeprom type (uint8_t)
 * @param type eeprom type (TypeEeprom)
 * @return none
 */
EEPROM::EEPROM(PinName sda, PinName scl, uint8_t address, TypeEeprom type) : _i2c(sda, scl)
{

  _errnum = EEPROM_NoError;
  _type = type;

  // Check address range
  _address = address;
  switch (type)
  {
  case T24C01:
  case T24C02:
    if (address > 7)
    {
      _errnum = EEPROM_BadAddress;
    }
    _address = _address << 1;
    _page_write = 8;
    _page_block_number = 1;
    break;
  case T24C04:
    if (address > 7)
    {
      _errnum = EEPROM_BadAddress;
    }
    _address = (_address & 0xFE) << 1;
    _page_write = 16;
    _page_block_number = 2;
    break;
  case T24C08:
    if (address > 7)
    {
      _errnum = EEPROM_BadAddress;
    }
    _address = (_address & 0xFC) << 1;
    _page_write = 16;
    _page_block_number = 4;
    break;
  case T24C16:
    _address = 0;
    _page_write = 16;
    _page_block_number = 8;
    break;
  case T24C32:
  case T24C64:
    if (address > 7)
    {
      _errnum = EEPROM_BadAddress;
    }
    _address = _address << 1;
    _page_write = 32;
    _page_block_number = 1;
    break;
  case T24C128:
  case T24C256:
    if (address > 7)
    {
      _errnum = EEPROM_BadAddress;
    }
    _address = _address << 1;
    _page_write = 64;
    _page_block_number = 1;
    break;
  case T24C512:
    if (address > 7)
    {
      _errnum = EEPROM_BadAddress;
    }
    _address = _address << 1;
    _page_write = 128;
    _page_block_number = 1;
    break;
  case T24C1024:
    if (address > 3)
    {
      _errnum = EEPROM_BadAddress;
    }
    _address = (_address & 0xFE) << 1;
    _page_write = 128;
    _page_block_number = 2;
    break;
  case T24C1025:
    if (address > 3)
    {
      _errnum = EEPROM_BadAddress;
    }
    _address = _address << 1;
    _page_write = 128;
    _page_block_number = 2;
    break;
  case M24M02:
    if (address > 1)
    {
      _errnum = EEPROM_BadAddress;
    }
    _address = _address << 3;
    _page_write = 256;
    _page_block_number = 4;
    break;  
  }

  // Size in bytes
  _size = _type;
  if (_type == T24C1025)
    _size = T24C1024;

  // Set I2C frequency
  _i2c.frequency(400000);
}

/**
 * void write(uint32_t address, int8_t data)
 *
 * Write byte
 * @param address start address (uint32_t)
 * @param data byte to write (int8_t)
 * @return none
 */
void EEPROM::write(uint32_t address, int8_t data)
{
  uint8_t page_block;
  uint8_t addr;
  uint8_t cmd[3];
  int len;
  int ack;

  // Check error
  if (_errnum)
    return;

  // Check address
  if (!checkAddress(address))
  {
    _errnum = EEPROM_OutOfRange;
    return;
  }

  // Compute page block
  page_block = 0;
  if (_type < T24C32)
  {
    page_block = address / 0xFF;
    address %= 0xFF;
  }
  else if (_type > T24C512)
  {
    page_block = address / 0xFFFF;
    address %= 0xFFFF;
  }

  // Device address
  addr = EEPROM_Address | _address | (page_block << 1);

  if (_type < T24C32)
  {
    len = 2;

    // Word address
    cmd[0] = (uint8_t)address;

    // Data
    cmd[1] = (uint8_t)data;
  }
  else
  {
    len = 3;

    // First word address (MSB)
    cmd[0] = (uint8_t)(address >> 8);

    // Second word address (LSB)
    cmd[1] = (uint8_t)address;

    // Data
    cmd[2] = (uint8_t)data;
  }

  ack = _i2c.write((int)addr, (char *)cmd, len);
  if (ack != 0)
  {
    _errnum = EEPROM_I2cError;
    return;
  }

  // Wait end of write
  ready();
}

/**
 * void write(uint32_t address, int8_t data[], uint32_t length)
 *
 * Write array of bytes (use the page mode)
 * @param address start address (uint32_t)
 * @param data bytes array to write (int8_t[])
 * @param size number of bytes to write (uint32_t)
 * @return none
 */
void EEPROM::write(uint32_t address, int8_t data[], uint32_t length)
{
  uint8_t page_block;
  uint8_t addr = 0;
  uint8_t blocs;
  uint16_t remain;
  uint8_t i, j;
  uint8_t cmd[MAX_PAGE_SIZE + 2];
  int ack;
  uint32_t written_cnt = 0;
  uint8_t len;

  // Check error
  if (_errnum)
    return;

  // Check address
  if (!checkAddress(address))
  {
    _errnum = EEPROM_OutOfRange;
    return;
  }

  // Check length
  if (!checkAddress(address + length - 1))
  {
    _errnum = EEPROM_OutOfRange;
    return;
  }

  // Consider offset for correct number of blocs
  auto offset = address % _page_write;

  // Compute blocs numbers
  blocs = (length + offset) / _page_write;

  // Compute remaining bytes
  remain = (length + offset) - blocs * _page_write;

  if (remain)
    blocs++;

  int32_t bytes_to_write = length;
  auto start_address = address;

  for (i = 0; i < blocs; i++)
  {
    // Compute page block
    page_block = 0;
    if (_type < T24C32)
    {
      page_block = address / 0xFF;
      address %= 0xFF;
    }
    else if (_type > T24C512)
    {
      page_block = address / 0xFFFF;
      address %= 0xFFFF;
    }

    // Device address
    addr = EEPROM_Address | _address | (page_block << 1);

    // Depending on the EEPROM the address can either be on one or two bytes
    if (_type < T24C32)
      len = 1;
    else
      len = 2;

    // Offset from start of page
    auto page_offset = address % _page_write;

    // Set the address part of cmd, in the case of the address on 2 bytes the MSB goes in the first element of cmd
    for (auto l = 0; l < len; l++)
      cmd[l] = (uint8_t)((address - page_offset) >> (8 * (len - l - 1)));

    // In case this is a partial write, read the whole page to refresh the untouched values
    if (page_offset != 0 || bytes_to_write < _page_write)
      read(address - page_offset, cmd + len, _page_write);

    // Loop  up to the page end or until there is data to read
    for (j = 0; (j < _page_write - page_offset) && (j < bytes_to_write); j++)
      cmd[j + page_offset + len] = (uint8_t)data[written_cnt + j];

    // Write data
    ack = _i2c.write((int)addr, (char *)cmd, _page_write + len);
    if (ack != 0)
    {
      _errnum = EEPROM_I2cError;
      return;
    }

    // Wait end of write
    ready();

    // Increment address and update the number of bytes written and to be written
    written_cnt += (_page_write - page_offset);
    address = start_address + written_cnt;
    bytes_to_write = length - written_cnt;
  }
}

/**
 * void write(uint32_t address, int16_t data)
 *
 * Write short
 * @param address start address (uint32_t)
 * @param data short to write (int16_t)
 * @return none
 */
void EEPROM::write(uint32_t address, int16_t data)
{
  int8_t cmd[2];

  // Check error
  if (_errnum)
    return;

  // Check address
  if (!checkAddress(address + 1))
  {
    _errnum = EEPROM_OutOfRange;
    return;
  }

  memcpy(cmd, &data, 2);

  write(address, cmd, 2);
}

/**
 * void write(uint32_t address, int32_t data)
 *
 * Write long
 * @param address start address (uint32_t)
 * @param data long to write (int32_t)
 * @return none
 */
void EEPROM::write(uint32_t address, int32_t data)
{
  int8_t cmd[4];

  // Check error
  if (_errnum)
    return;

  // Check address
  if (!checkAddress(address + 3))
  {
    _errnum = EEPROM_OutOfRange;
    return;
  }

  memcpy(cmd, &data, 4);

  write(address, cmd, 4);
}

/**
 * void write(uint32_t address, float data)
 *
 * Write float
 * @param address start address (uint32_t)
 * @param data float to write (float)
 * @return none
 */
void EEPROM::write(uint32_t address, float data)
{
  int8_t cmd[4];

  // Check error
  if (_errnum)
    return;

  // Check address
  if (!checkAddress(address + 3))
  {
    _errnum = EEPROM_OutOfRange;
    return;
  }

  memcpy(cmd, &data, 4);

  write(address, cmd, 4);
}

/**
 * void write(uint32_t address, void *data, uint32_t size)
 *
 * Write anything (use the page write mode)
 * @param address start address (uint32_t)
 * @param data data to write (void *)
 * @param size number of bytes to write (uint32_t)
 * @return none
 */
void EEPROM::write(uint32_t address, void *data, uint32_t size)
{
  int8_t *cmd = NULL;

  // Check error
  if (_errnum)
    return;

  // Check address
  if (!checkAddress(address + size - 1))
  {
    _errnum = EEPROM_OutOfRange;
    return;
  }

  cmd = (int8_t *)malloc(size);
  if (cmd == NULL)
  {
    _errnum = EEPROM_MallocError;
    return;
  }

  memcpy(cmd, (uint8_t *)data, size);

  write(address, cmd, size);

  free(cmd);
}

/**
 * void read(uint32_t address, int8_t& data)
 *
 * Random read byte
 * @param address start address (uint32_t)
 * @param data byte to read (int8_t&)
 * @return none
 */
void EEPROM::read(uint32_t address, int8_t &data)
{
  uint8_t page_block;
  uint8_t addr;
  uint8_t cmd[2];
  uint8_t len;
  int ack;

  // Check error
  if (_errnum)
    return;

  // Check address
  if (!checkAddress(address))
  {
    _errnum = EEPROM_OutOfRange;
    return;
  }

  // Compute page block
  page_block = 0;
  if (_type < T24C32)
  {
    page_block = address / 0xFF;
    address %= 0xFF;
  }
  else if (_type > T24C512)
  {
    page_block = address / 0xFFFF;
    address %= 0xFFFF;
  }

  // Device address
  addr = EEPROM_Address | _address | (page_block << 1);

  // Depending on the EEPROM the address can either be on one or two bytes
  if (_type < T24C32)
    len = 1;
  else
    len = 2;

  // Set the address part of cmd, in the case of the address on 2 bytes the MSB goes in the first element of cmd
  for (auto l = 0; l < len; l++)
    cmd[l] = (uint8_t)((address) >> (8 * (len - l - 1)));

  // Write command
  ack = _i2c.write((int)addr, (char *)cmd, len, true);
  if (ack != 0)
  {
    _errnum = EEPROM_I2cError;
    return;
  }

  // Read data
  ack = _i2c.read((int)addr, (char *)&data, sizeof(data));
  if (ack != 0)
  {
    _errnum = EEPROM_I2cError;
    return;
  }
}

/**
 * void read(uint32_t address, int8_t *data, uint32_t size)
 *
 * Sequential read byte
 * @param address start address (uint32_t)
 * @param data bytes array to read (int8_t[]&)
 * @param size number of bytes to read (uint32_t)
 * @return none
 */
void EEPROM::read(uint32_t address, int8_t *data, uint32_t size)
{
  uint8_t page_block;
  uint8_t addr;
  uint8_t cmd[2];
  uint8_t len;
  int ack;

  // Check error
  if (_errnum)
    return;

  // Check address
  if (!checkAddress(address))
  {
    _errnum = EEPROM_OutOfRange;
    return;
  }

  // Check size
  if (!checkAddress(address + size - 1))
  {
    _errnum = EEPROM_OutOfRange;
    return;
  }

  // Compute page block
  page_block = 0;
  if (_type < T24C32)
  {
    page_block = address / 0xFF;
    address %= 0xFF;
  }
  else if (_type > T24C512)
  {
    page_block = address / 0xFFFF;
    address %= 0xFFFF;
  }

  // Device address
  addr = EEPROM_Address | _address | (page_block << 1);

  // Depending on the EEPROM the address can either be on one or two bytes
  if (_type < T24C32)
    len = 1;
  else
    len = 2;

  // Set the address part of cmd, in the case of the address on 2 bytes the MSB goes in the first element of cmd
  for (auto l = 0; l < len; l++)
    cmd[l] = (uint8_t)((address) >> (8 * (len - l - 1)));

  // Write command
  ack = _i2c.write((int)addr, (char *)cmd, len, true);
  if (ack != 0)
  {
    _errnum = EEPROM_I2cError;
    return;
  }

  // Sequential read
  ack = _i2c.read((int)addr, (char *)data, size);
  if (ack != 0)
  {
    _errnum = EEPROM_I2cError;
    return;
  }
}

/**
 * void read(int8_t& data)
 *
 * Current address read byte
 * @param data byte to read (int8_t&)
 * @return none
 */
void EEPROM::read(int8_t &data)
{
  uint8_t page_block;
  uint8_t addr;
  int ack;

  // Check error
  if (_errnum)
    return;

  // Device address
  addr = EEPROM_Address | _address;

  // Read data
  ack = _i2c.read((int)addr, (char *)&data, sizeof(data));
  if (ack != 0)
  {
    _errnum = EEPROM_I2cError;
    return;
  }
}

/**
 * void read(uint32_t address, int16_t& data)
 *
 * Random read short
 * @param address start address (uint32_t)
 * @param data short to read (int16_t&)
 * @return none
 */
void EEPROM::read(uint32_t address, int16_t &data)
{
  int8_t cmd[2];

  // Check error
  if (_errnum)
    return;

  // Check address
  if (!checkAddress(address + 1))
  {
    _errnum = EEPROM_OutOfRange;
    return;
  }

  read(address, cmd, 2);

  memcpy(&data, cmd, 2);
}

/**
 * void read(uint32_t address, int32_t& data)
 *
 * Random read long
 * @param address start address (uint32_t)
 * @param data long to read (int32_t&)
 * @return none
 */
void EEPROM::read(uint32_t address, int32_t &data)
{
  int8_t cmd[4];

  // Check error
  if (_errnum)
    return;

  // Check address
  if (!checkAddress(address + 3))
  {
    _errnum = EEPROM_OutOfRange;
    return;
  }

  read(address, cmd, 4);

  memcpy(&data, cmd, 4);
}

/**
 * void read(uint32_t address, float& data)
 *
 * Random read float
 * @param address start address (uint32_t)
 * @param data float to read (float&)
 * @return none
 */
void EEPROM::read(uint32_t address, float &data)
{
  int8_t cmd[4];

  // Check error
  if (_errnum)
    return;

  // Check address
  if (!checkAddress(address + 3))
  {
    _errnum = EEPROM_OutOfRange;
    return;
  }

  read(address, cmd, 4);

  memcpy(&data, cmd, 4);
}

/**
 * void read(uint32_t address, void *data, uint32_t size)
 *
 * Random read anything
 * @param address start address (uint32_t)
 * @param data data to read (void *)
 * @param size number of bytes to read (uint32_t)
 * @return none
 */
void EEPROM::read(uint32_t address, void *data, uint32_t size)
{
  int8_t *cmd = NULL;

  // Check error
  if (_errnum)
    return;

  // Check address
  if (!checkAddress(address + size - 1))
  {
    _errnum = EEPROM_OutOfRange;
    return;
  }

  cmd = (int8_t *)malloc(size);

  if (cmd == NULL)
  {
    _errnum = EEPROM_MallocError;
    return;
  }

  read(address, (int8_t *)cmd, size);

  memcpy(data, cmd, size);

  free(cmd);
}

/**
 * void clear(void)
 *
 * Clear eeprom (write with 0)
 * @param none
 * @return none
 */
void EEPROM::clear(void)
{
  int32_t data;
  uint32_t i;

  data = 0;

  for (i = 0; i < _size / 4; i++)
  {
    write((uint32_t)(i * 4), data);
  }
}

/**
 * void ready(void)
 *
 * Wait eeprom ready
 * @param none
 * @return none
 */
void EEPROM::ready(void)
{
  int ack;
  uint8_t addr;
  uint8_t cmd[2];

  // Check error
  if (_errnum)
    return;

  // Device address
  addr = EEPROM_Address | _address;

  cmd[0] = 0;

  // Wait end of write
  do
  {
    ack = _i2c.write((int)addr, (char *)cmd, 0);
    // wait(0.5);
  } while (ack != 0);
}

/**
 * uint32_t getSize(void)
 *
 * Get eeprom size in bytes
 * @param  none
 * @return size in bytes (uint32_t)
 */
uint32_t EEPROM::getSize(void)
{
  return (_size);
}

/**
 * const char* getName(void)
 *
 * Get eeprom name
 * @param none
 * @return name (const char*)
 */
const char *EEPROM::getName(void)
{
  uint8_t i = 0;

  switch (_type)
  {
  case T24C01:
    i = 0;
    break;
  case T24C02:
    i = 1;
    break;
  case T24C04:
    i = 2;
    break;
  case T24C08:
    i = 3;
    break;
  case T24C16:
    i = 4;
    break;
  case T24C32:
    i = 5;
    break;
  case T24C64:
    i = 6;
    break;
  case T24C128:
    i = 7;
    break;
  case T24C256:
    i = 8;
    break;
  case T24C512:
    i = 9;
    break;
  case T24C1024:
    i = 10;
    break;
  case T24C1025:
    i = 11;
    break;
  case M24M02:
    i = 12;
    break;
  }

  return (_name[i]);
}

/**
 * uint8_t getError(void)
 *
 * Get the current error number (EEPROM_NoError if no error)
 * @param none
 * @return none
 */
uint8_t EEPROM::getError(void)
{
  return (_errnum);
}

/**
 * bool checkAddress(uint32_t address)
 *
 * Check if address is in the eeprom range address
 * @param address address to check (uint32_t)
 * @return true if in eeprom range, overwise false (bool)
 */
bool EEPROM::checkAddress(uint32_t address)
{
  bool ret = true;

  switch (_type)
  {
  case T24C01:
    if (address >= T24C01)
      ret = false;
    break;
  case T24C02:
    if (address >= T24C02)
      ret = false;
    break;
  case T24C04:
    if (address >= T24C04)
      ret = false;
    break;
  case T24C08:
    if (address >= T24C08)
      ret = false;
    break;
  case T24C16:
    if (address >= T24C16)
      ret = false;
    break;
  case T24C32:
    if (address >= T24C32)
      ret = false;
    break;
  case T24C64:
    if (address >= T24C64)
      ret = false;
    break;
  case T24C128:
    if (address >= T24C128)
      ret = false;
    break;
  case T24C256:
    if (address >= T24C256)
      ret = false;
    break;
  case T24C512:
    if (address >= T24C512)
      ret = false;
    break;
  case T24C1024:
    if (address >= T24C1024)
      ret = false;
    break;
  case T24C1025:
    if (address >= T24C1025 - 1)
      ret = false;
    break;
  case M24M02:
    if (address >= M24M02 - 1)
      ret = false;
    break;
  }

  return (ret);
}
