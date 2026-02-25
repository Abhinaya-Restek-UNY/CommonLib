
#pragma once
#include "I2CDevice.hpp"
#include "driver/i2c_master.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "lwip/err.h"
#include "time.h"
#include <cmath>

#define ANGLE_TRESHOLD 0.8

uint64_t I2CDevice::get_time() { return esp_timer_get_time(); }
I2CDevice::I2CDevice(i2c_master_bus_handle_t &bus, uint16_t device_address) {
  //
  i2c_device_config_t dev_conf = {.dev_addr_length = I2C_ADDR_BIT_LEN_7,
                                  .device_address = device_address,
                                  .scl_speed_hz = 100'000};
  ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_conf, &device_handle));
}
int8_t I2CDevice::readBit(uint8_t regAddr, uint8_t bitNum, uint8_t *data,
                          uint16_t timeout) {
  uint8_t b;
  uint8_t count = readByte(regAddr, &b, timeout);
  *data = b & (1 << bitNum);
  return count;
};
int8_t I2CDevice::readBits(uint8_t regAddr, uint8_t bitStart, uint8_t length,
                           uint8_t *data, uint16_t timeout) {
  // 01101001 read byte
  // 76543210 bit numbers
  //    xxx   args: bitStart=4, length=3
  //    010   masked
  //   -> 010 shifted
  uint8_t count, b;
  if ((count = readByte(regAddr, &b, timeout)) != 0) {
    uint8_t mask = ((1 << length) - 1) << (bitStart - length + 1);
    b &= mask;
    b >>= (bitStart - length + 1);
    *data = b;
  }
  return count;
}

int8_t I2CDevice::readWord(uint8_t regAddr, uint16_t *data, uint16_t timeout) {
  uint8_t msb[2] = {0, 0};
  readBytes(regAddr, 2, msb);
  *data = (int16_t)((msb[0] << 8) | msb[1]);
  return 0;
}

int8_t I2CDevice::readByte(uint8_t regAddr, uint8_t *data, uint16_t timeout) {
  return readBytes(regAddr, 1, data, timeout);
}

int8_t I2CDevice::readBytes(uint8_t regAddr, uint8_t length, uint8_t *data,
                            uint16_t timeout) {
  esp_err_t err = (i2c_master_transmit_receive(this->device_handle, &regAddr, 1,
                                               data, length, timeout));
  // vTaskDelay(5); /// dont know why it works.
  if (err == ESP_OK) {
    return length;
  }

  // ESP_ERROR_CHECK(err);

  return -1;
}

bool I2CDevice::writeBit(uint8_t regAddr, uint8_t bitNum, uint8_t data) {
  uint8_t b;
  readByte(regAddr, &b);
  b = (data != 0) ? (b | (1 << bitNum)) : (b & ~(1 << bitNum));
  return writeByte(regAddr, b);
}

bool I2CDevice::writeBits(uint8_t regAddr, uint8_t bitStart, uint8_t length,
                          uint8_t data) {
  uint8_t b = 0;
  if (this->readByte(regAddr, &b) != 0) {
    uint8_t mask = ((1 << length) - 1) << (bitStart - length + 1);
    data <<= (bitStart - length + 1); // shift data into correct position
    data &= mask;                     // zero all non-important bits in data
    b &= ~(mask); // zero all important bits in existing byte
    b |= data;    // combine data with existing byte
    return writeByte(regAddr, b);
  } else {
    return false;
  }
}

void I2CDevice::delay(uint16_t delay) {
  return vTaskDelay(pdMS_TO_TICKS(delay));
}
bool I2CDevice::writeByte(uint8_t reg, uint8_t byte) {
  return this->writeBytes(reg, 1, &byte);
}

bool I2CDevice::writeWord(uint8_t regAddr, uint16_t data) {
  uint8_t data1[] = {(uint8_t)(data >> 8), (uint8_t)(data & 0xff)};
  return this->writeBytes(regAddr, 2, data1);
};

bool I2CDevice::writeBytes(uint8_t regAddr, uint8_t length, uint8_t *data) {
  i2c_master_transmit_multi_buffer_info_t payload[2] = {
      {.write_buffer = &regAddr, .buffer_size = 1},
      {.write_buffer = data, .buffer_size = length},
  };

  esp_err_t err = (i2c_master_multi_buffer_transmit(
      this->device_handle, payload,
      sizeof(payload) / sizeof(i2c_master_transmit_multi_buffer_info_t), -1));

  if (err == ESP_OK) {
    return true;
  }

  return false;
}

uint32_t I2CDevice::get_time_mil() { return this->get_time() / 1000; }

int8_t I2CDevice::readWords(uint8_t regAddr, uint8_t length, uint16_t *data,
                            uint16_t timeout) {

  uint8_t total_byte = length * 2;
  int8_t ret = this->readBytes(regAddr, total_byte, (uint8_t *)data);

  if (ret < 0) {
    return ret;
  }

  uint8_t wordI = 0;
  for (uint8_t i = 0; i < total_byte; i += 2) {
    uint8_t msb = ((uint8_t *)data)[i];
    uint8_t lsb = ((uint8_t *)data)[i + 1];

    data[wordI] = (uint16_t)(msb << 8) | lsb;

    wordI++;
  }

  return length;
}
bool I2CDevice::writeWords(uint8_t regAddr, uint8_t length, uint16_t *data) {

  uint8_t total_byte = length * 2;
  i2c_master_transmit_multi_buffer_info_t *payload =
      new i2c_master_transmit_multi_buffer_info_t[total_byte + 1];
  payload[0].buffer_size = 1;
  payload[0].write_buffer = &regAddr;

  for (uint8_t i = 0; i < total_byte; i += 2) {

    i2c_master_transmit_multi_buffer_info_t &msb = payload[i + 1];
    i2c_master_transmit_multi_buffer_info_t &lsb = payload[i + 2];

    msb.buffer_size = 1;
    lsb.buffer_size = 1;
    msb.write_buffer = &((uint8_t *)data)[i + 1]; // MSB
    lsb.write_buffer = &((uint8_t *)data)[i];     // LSB
  }

  esp_err_t err = (i2c_master_multi_buffer_transmit(
      this->device_handle, payload, total_byte + 1, -1));

  delete[] payload;
  if (err == ESP_OK) {
    return true;
  }

  return false;
};

I2CDevice::~I2CDevice() { i2c_master_bus_rm_device(this->device_handle); };
