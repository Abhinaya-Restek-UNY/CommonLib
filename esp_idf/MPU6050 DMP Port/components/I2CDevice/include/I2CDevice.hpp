#include "driver/i2c_types.h"
#include <sys/_timeval.h>

#define I2CDEVLIB_WIRE_BUFFER_LENGTH 256

class I2CDevice {
public:
  I2CDevice(i2c_master_bus_handle_t &bus, uint16_t device_address);
  ~I2CDevice();

private:
  i2c_master_dev_handle_t device_handle;

protected:
  void delay(uint16_t delay);
  uint64_t get_time();
  uint32_t get_time_mil();
  int8_t readBit(uint8_t regAddr, uint8_t bitNum, uint8_t *data,
                 uint16_t timeout = I2CDevice::readTimeout);
  // TODO static int8_t readBitW(uint8_t regAddr, uint8_t
  // bitNum, uint16_t *data, uint16_t timeout=I2CDevice::readTimeout);
  int8_t readBits(uint8_t regAddr, uint8_t bitStart, uint8_t length,
                  uint8_t *data, uint16_t timeout = I2CDevice::readTimeout);
  // TODO static int8_t readBitsW(uint8_t regAddr, uint8_t
  // bitStart, uint8_t length, uint16_t *data, uint16_t
  // timeout=I2CDevice::readTimeout);
  int8_t readByte(uint8_t regAddr, uint8_t *data,
                  uint16_t timeout = I2CDevice::readTimeout);
  int8_t readWord(uint8_t regAddr, uint16_t *data,
                  uint16_t timeout = I2CDevice::readTimeout);
  int8_t readWords(uint8_t regAddr, uint8_t length, uint16_t *data,
                   uint16_t timeout = I2CDevice::readTimeout);
  int8_t readBytes(uint8_t regAddr, uint8_t length, uint8_t *data,
                   uint16_t timeout = I2CDevice::readTimeout);
  // TODO static int8_t readWords(uint8_t regAddr, uint8_t
  // length, uint16_t *data, uint16_t timeout=I2Cdev::readTimeout);

  bool writeBit(uint8_t regAddr, uint8_t bitNum, uint8_t data);
  // TODO static bool writeBitW(uint8_t regAddr, uint8_t
  // bitNum, uint16_t data);
  bool writeBits(uint8_t regAddr, uint8_t bitStart, uint8_t length,
                 uint8_t data);
  // TODO static bool writeBitsW(uint8_t regAddr, uint8_t
  // bitStart, uint8_t length, uint16_t data);
  bool writeByte(uint8_t regAddr, uint8_t data);
  bool writeWord(uint8_t regAddr, uint16_t data);
  bool writeWords(uint8_t regAddr, uint8_t length, uint16_t *data);
  bool writeBytes(uint8_t regAddr, uint8_t length, uint8_t *data);

  static const uint16_t readTimeout = 1000;
  // TODO static bool writeWords(uint8_t regAddr, uint8_t
  // length, uint16_t *data);
};
