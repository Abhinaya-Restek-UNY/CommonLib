#pragma once
#define TUNNEL_TAG "TUNNEL"
#include "UDPHandler.hpp"
#include "freertos/idf_additions.h"
#include "mbedtls/ssl.h"
#include <vector>

#define MAX_HANDSHAKE 10

enum TUNNEL_STATE {
  TUNNEL_STATE_CONNECTION_LOST,
  TUNNEL_STATE_HANDSHAKE_IS_YET,
  TUNNEL_STATE_HANDSHAKE_IS_ERROR,
  TUNNEL_STATE_HANDSHAKE_IN_PROGRESS,
  TUNNEL_STATE_HANDSHAKE_IS_DONE
};

struct TimerData {
  uint32_t start;
  uint32_t intermediate;
  uint32_t final;
};

class Tunnel {
public:
  Tunnel(std::string ip_address, uint16_t port, UDPHandler &socket,
         mbedtls_ssl_config &cfg);
  ~Tunnel();
  Tunnel(const Tunnel &) = delete;
  Tunnel &operator=(const Tunnel &) = delete;
  Tunnel(const Tunnel &&) = delete;
  Tunnel &operator=(const Tunnel &&) = delete;

  QueueHandle_t get_read_queue();

  ssize_t write(unsigned char *data, size_t length);
  ssize_t read(unsigned char *buffer, size_t length);

  int handshake();

  TUNNEL_STATE get_state();

private:
  static ssize_t dtls_read(Tunnel *_this, unsigned char *buffer,
                           size_t buffer_size);

  static ssize_t dtls_write(Tunnel *_this, const unsigned char *buffer,
                            size_t buffer_size);

  static ssize_t dtls_read_with_timeout(Tunnel *_this, unsigned char *buffer,
                                        size_t buffer_size,
                                        uint32_t timeout_ms);

  static void mbedtls_timing_set_delay(void *ctx, uint32_t int_ms,
                                       uint32_t fin_ms);

  static int mbedtls_timing_get_delay(void *ctx);

  uint32_t get_time_ms();

  struct timeval time_track;

  TUNNEL_STATE tunnel_state = TUNNEL_STATE_HANDSHAKE_IS_YET;
  TimerData timer = {0, 0, 0};

  UDPHandler &udp_handler;

  Address address;

  mbedtls_ssl_context ssl_context = {};
  QueueHandle_t read_queue;
};
