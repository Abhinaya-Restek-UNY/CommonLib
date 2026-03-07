
#include "serial_hub.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

#define TEST_SIZE 30

std::vector<uint8_t> write_buf;

void local_write(void *ctx, uint8_t *data, fsize_t size) {
  serial_hub_handle_t *dst = (serial_hub_handle_t *)ctx;
  write_buf.insert(write_buf.end(), data, data + size);
};

fsize_t well = 0;
void local_on_read(uint8_t *data, fsize_t size) {
  well++;
  printf("%d reading data %d\n", well, size);
};

int main(int argc, char *argv[]) {
  serial_hub_handle_t handle1;
  serial_hub_handle_t handle2;

  serial_hub_initialize(&handle1, local_write, &handle2);
  serial_hub_initialize(&handle2, local_write, &handle1);

  uint8_t dat[TEST_SIZE] = {};

  for (fsize_t t = 1; t < TEST_SIZE; t++) {
    dat[t] = t % 254;
  }

  // serial_hub_reserve_memory(&handle1, TEST_SIZE);
  // serial_hub_reserve_memory(&handle2, TEST_SIZE);
  serial_hub_attach_topic(&handle2, 1, sizeof(dat), local_on_read);

  for (fsize_t i = 0; i < 100; i++) {
    serial_hub_write_topic(&handle1, 1, (uint8_t *)&dat, TEST_SIZE);
  }

  std::random_device rd;

  std::mt19937 gen(rd());

  std::uniform_int_distribution<> distrib(25, 50);

  fsize_t fuck = 0;
  while (fuck < write_buf.size()) {

    fsize_t advance = fuck + distrib(gen);
    if (advance > write_buf.size()) {
      advance = write_buf.size();
    }
    serial_hub_on_read(&handle2, &write_buf[fuck], advance - fuck);
    fuck = advance;
  }

  return 0;
}
