
#include "serial_hub.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <string.h>
#include <vector>

#define TEST_SIZE 24
#include <stdint.h>
#include <stdio.h>

inline void print_hex_dump(const uint8_t *data, size_t size) {
  for (fsize_t i = 0; i < size; i++) {
    printf("%02X ", data[i]);

    // Print a newline every 16 bytes for readability
    if ((i + 1) % 16 == 0) {
      printf("\n");
    }
  }
  // Add a final newline if the last line didn't end perfectly on a 16-byte
  // boundary
  if (size % 16 != 0) {
    printf("\n");
  }
}

std::vector<uint8_t> write_buf;

void local_write(void *ctx, uint8_t *data, fsize_t size) {
  serial_hub_handle_t *dst = (serial_hub_handle_t *)ctx;
  print_hex_dump(data, size);
  fsize_t first = size / 4;
  fsize_t middle = first * 3;
  serial_hub_on_read(dst, data, size);
};

fsize_t well = 0;
void local_on_read(void *ctx, uint8_t *data, fsize_t size) {
  well++;
  printf("\n%d reading data %d\n", well, size);
  print_hex_dump(data, size);
};

int main(int argc, char *argv[]) {
  serial_hub_handle_t handle1;
  serial_hub_handle_t handle2;

  serial_hub_initialize(&handle1, local_write, &handle2);
  serial_hub_initialize(&handle2, local_write, &handle1);
  serial_hub_reserve_memory(&handle1, TEST_SIZE);
  serial_hub_reserve_memory(&handle2, TEST_SIZE);

  uint8_t dat[TEST_SIZE + 3] = {};

  for (fsize_t t = 0; t < TEST_SIZE + 3; t++) {
    dat[t] = (t % 254) + 1;
    if (dat[t] % 5 == 0) {
      dat[t] = 0x0;
    }
  }

  printf("\n");
  printf("original:\n");
  print_hex_dump(dat, TEST_SIZE);
  printf("\n");

  // serial_hub_reserve_memory(&handle1, TEST_SIZE);
  // serial_hub_reserve_memory(&handle2, TEST_SIZE);

  serial_hub_attach_topic(&handle2, 1, sizeof(dat), NULL, local_on_read);

  serial_hub_write_topic(&handle1, 1, (uint8_t *)&dat, TEST_SIZE + 3);

  return 0;
}
