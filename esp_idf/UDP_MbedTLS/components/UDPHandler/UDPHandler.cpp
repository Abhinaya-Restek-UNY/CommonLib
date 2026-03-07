#include "UDPHandler.hpp"
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include <cstring>

UDPHandler::UDPHandler(uint16_t port) : read_subscriber() {

  struct sockaddr_in dest_addr_ip4{};
  dest_addr_ip4.sin_addr.s_addr = htonl(INADDR_ANY);
  dest_addr_ip4.sin_family = AF_INET;
  dest_addr_ip4.sin_port = htons(port);
  this->sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);

  this->err = bind(this->sockfd, (struct sockaddr *)&dest_addr_ip4,
                   sizeof(dest_addr_ip4));
  this->read_subscriber_mutex = xSemaphoreCreateMutex();

  xTaskCreate((TaskFunction_t)this->vReadTask, "UDP Read T.", 2048, this, 1,
              &this->read_task_handle);
}

ssize_t UDPHandler::write(const Address &to_addr, const unsigned char *data,
                          size_t data_len) {

  ESP_LOGI(UDP_TAG, "Writing %d byte to %s", data_len,
           inet_ntoa(to_addr.sin_addr.s_addr));
  return sendto(this->sockfd, data, data_len, MSG_DONTWAIT,
                (const struct sockaddr *)&to_addr, sizeof(to_addr));
}

Address UDPHandler::gen_address(std::string ip, uint16_t port) {
  struct sockaddr_in address = {
      .sin_family = AF_INET,
      .sin_port = htons(port),
      .sin_addr = {.s_addr = inet_addr(ip.c_str())},
  };

  return (address);
};

void UDPHandler::vReadTask(UDPHandler *_this) {
  uint8_t data[1024];

  Address from_addr;
  socklen_t addr_len = sizeof(from_addr);
  int ret = 0;

  while (1) {

    ret = recvfrom(_this->sockfd, data, sizeof(data), 0,
                   (struct sockaddr *)(&from_addr), &addr_len);
    if (ret > 0) {
      if (xSemaphoreTake(_this->read_subscriber_mutex, portMAX_DELAY) ==
          pdTRUE) {
        uint64_t key = _this->get_subscriber_key(from_addr);
        auto it = _this->read_subscriber.find(key);

        if (it == _this->read_subscriber.end()) {

          xSemaphoreGive(_this->read_subscriber_mutex);
          if (_this->address_unknown_cb != NULL) {
            _this->address_unknown_cb(from_addr, _this->address_unknown_cb_ctx);
          }

        } else {
          uint8_t *transport_buf = (uint8_t *)malloc(ret);
          memcpy(transport_buf, data, ret);
          Packet transport_packet{.data = transport_buf, .data_len = ret};
          xQueueSend(it->second, &transport_packet, portMAX_DELAY);
          xSemaphoreGive(_this->read_subscriber_mutex);
        }
      }
    };
  }
}

int8_t UDPHandler::subscribe_read(Address address, QueueHandle_t queue) {
  if (xSemaphoreTake(this->read_subscriber_mutex, portMAX_DELAY) == pdTRUE) {
    this->read_subscriber.emplace(this->get_subscriber_key(address), queue);
    xSemaphoreGive(this->read_subscriber_mutex);
    return 0;
  }

  return -1;
}
int8_t UDPHandler::set_on_unknown_address(on_unknown_address_cb callback,
                                          void *ctx) {
  this->address_unknown_cb_ctx = ctx;
  this->address_unknown_cb = callback;
  return 0;
}

int8_t UDPHandler::unsubscribe_read(Address address) {
  if (xSemaphoreTake(this->read_subscriber_mutex, portMAX_DELAY) == pdTRUE) {
    this->read_subscriber.erase(this->get_subscriber_key(address));
    xSemaphoreGive(this->read_subscriber_mutex);
    return 0;
  }

  return -1;
};

uint64_t UDPHandler::get_subscriber_key(Address &address) {
  uint64_t key = 0;
  key |= address.sin_addr.s_addr;
  return ((key << sizeof(uint16_t)) | address.sin_port);
}
