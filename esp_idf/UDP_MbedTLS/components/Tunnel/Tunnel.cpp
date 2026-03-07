#include "Tunnel.hpp"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "mbedtls/ssl.h"
#include <cstring>
#include <stdlib.h>
#include <string>
#include <sys/time.h>

Tunnel::Tunnel(std::string ip_address, uint16_t port, UDPHandler &socket,
               mbedtls_ssl_config &cfg)
    : udp_handler(socket) {
  this->read_queue = xQueueCreate(4, sizeof(Packet));
  this->tunnel_state = TUNNEL_STATE_HANDSHAKE_IS_YET;
  this->address = socket.gen_address(ip_address, port);

  mbedtls_ssl_init(&this->ssl_context);

  mbedtls_ssl_setup(&this->ssl_context, &cfg);
  mbedtls_ssl_set_bio(
      &(this->ssl_context), (void *)(this),
      (mbedtls_ssl_send_t *)this->dtls_write,
      (mbedtls_ssl_recv_t *)this->dtls_read,
      (mbedtls_ssl_recv_timeout_t *)this->dtls_read_with_timeout);

  mbedtls_ssl_set_timer_cb(&(this->ssl_context), (void *)(this),
                           this->mbedtls_timing_set_delay,
                           this->mbedtls_timing_get_delay);

  socket.subscribe_read(this->address, this->read_queue);
}

ssize_t Tunnel::dtls_read(Tunnel *_this, unsigned char *buffer,
                          size_t buffer_size) {
  Packet packet;
  if (xQueueReceive(_this->read_queue, &packet, portMAX_DELAY) != pdPASS) {
    return MBEDTLS_ERR_SSL_WANT_READ;
  };
  if (!packet.data_len) {
    // TODO: Check for other error
    return MBEDTLS_ERR_SSL_WANT_READ;
  }

  int16_t cpy_len =
      buffer_size > packet.data_len ? packet.data_len : buffer_size;

  memcpy(buffer, packet.data, cpy_len);

  free(packet.data);

  return cpy_len;
};

ssize_t Tunnel::dtls_write(Tunnel *_this, const unsigned char *data,
                           size_t data_size) {
  int ret = -1;
  if ((ret = _this->udp_handler.write(_this->address, data, data_size)) > 0) {
    return ret;
  }
  return MBEDTLS_ERR_SSL_WANT_WRITE;
};

ssize_t Tunnel::dtls_read_with_timeout(Tunnel *_this, unsigned char *buffer,
                                       size_t buffer_size,
                                       uint32_t timeout_ms) {

  Packet packet;
  if (xQueueReceive(_this->read_queue, &packet, pdMS_TO_TICKS(timeout_ms)) ==
      pdTRUE) {

    int16_t cpy_len =
        buffer_size > packet.data_len ? packet.data_len : buffer_size;

    memcpy(buffer, packet.data, cpy_len);

    free(packet.data);

    return cpy_len;
  };

  return MBEDTLS_ERR_SSL_TIMEOUT;
};

QueueHandle_t Tunnel::get_read_queue() { return this->read_queue; }

uint32_t Tunnel::get_time_ms() {
  gettimeofday(&this->time_track, NULL);
  return (this->time_track.tv_sec * 1000LL +
          (this->time_track.tv_usec / 1000LL));
}

void Tunnel::mbedtls_timing_set_delay(void *ctx, uint32_t int_ms,
                                      uint32_t fin_ms) {
  Tunnel *_this = (Tunnel *)ctx;
  TimerData &timer = _this->timer;
  if (fin_ms == 0) {
    timer.final = 0;
    return;
  }

  timer.start = _this->get_time_ms();
  timer.intermediate = int_ms;
  timer.final = fin_ms;
};

int Tunnel::mbedtls_timing_get_delay(void *ctx) {
  Tunnel *_this = (Tunnel *)ctx;
  TimerData &timer = _this->timer;
  if (timer.final == 0) {
    return -1;
  }

  uint32_t delta = _this->get_time_ms() - timer.start;

  if (timer.final <= delta) {
    return 2;
  }

  if (timer.intermediate <= delta) {
    return 1;
  }

  return 0;
}

TUNNEL_STATE Tunnel::get_state() { return this->tunnel_state; }

int Tunnel::handshake() {

  // if (this->tunnel_state == TUNNEL_STATE_HANDSHAKE_IN_PROGRESS) {
  //   return 0;
  // }

  this->tunnel_state = TUNNEL_STATE_HANDSHAKE_IN_PROGRESS;

  int ret = 0;
  // do {
  ret = mbedtls_ssl_handshake(&this->ssl_context);

  if (ret == 0) {
    this->tunnel_state = TUNNEL_STATE_HANDSHAKE_IS_DONE;
    return 0;
  }

  if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE ||
      ret == MBEDTLS_ERR_SSL_TIMEOUT) {
    // Non-blocking I/O, retry later
    return 1;
  }

  // ❌ Fatal error
  this->tunnel_state = TUNNEL_STATE_HANDSHAKE_IS_ERROR;

  return 0;
}

ssize_t Tunnel::write(unsigned char *data, size_t length) {

  return mbedtls_ssl_write(&this->ssl_context, data, length);
}
ssize_t Tunnel::read(unsigned char *buffer, size_t length) {
  return mbedtls_ssl_read(&this->ssl_context, buffer, length);
}

Tunnel::~Tunnel() {
  mbedtls_ssl_free(&this->ssl_context);
  vQueueDelete(this->read_queue);
  this->udp_handler.unsubscribe_read(this->address);
}
