#pragma once
#include "lwip/sockets.h"
#include <string>
#include <unordered_map>
#define UDP_TAG "UDP"

typedef struct sockaddr_in Address;
using on_unknown_address_cb = void (*)(Address address, void *ctx);

struct Packet {
  uint8_t *data;
  ssize_t data_len;
};

class UDPHandler {
public:
  UDPHandler(uint16_t port);

  ssize_t write(const Address &to_addr, const unsigned char *data,
                size_t data_len);

  // ssize_t read(Address &from_addr, unsigned char *data, size_t data_len);

  Address gen_address(std::string ip, uint16_t port);

  int8_t subscribe_read(Address address, QueueHandle_t queue);
  int8_t unsubscribe_read(Address address);
  int8_t set_on_unknown_address(on_unknown_address_cb callback, void *ctx);

private:
  on_unknown_address_cb address_unknown_cb = NULL;
  void *address_unknown_cb_ctx = NULL;

  uint16_t port = 4444;
  int sockfd = 0;
  int err = 0;

  std::unordered_map<uint64_t, QueueHandle_t> read_subscriber;
  SemaphoreHandle_t read_subscriber_mutex;

  static void vReadTask(UDPHandler *ctx);
  TaskHandle_t read_task_handle;

  static uint64_t get_subscriber_key(Address &address);
};
