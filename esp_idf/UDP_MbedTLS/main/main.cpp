#include "Tunnel.hpp"
#include "WifiHandler.hpp"
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ssl.h"

inline unsigned int s2tick(unsigned int s) { return pdTICKS_TO_MS(s * 10); }

struct mbedtls_config_data {
  mbedtls_ssl_config conf = {};
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctr_drbg;
};

void gen_mbedtls_config(mbedtls_config_data *data, std::string username,
                        std::string password) {
  ESP_LOGI("GEN_MBEDTLS", "Setting up mbedtls config...");
  mbedtls_ssl_config_init(&data->conf);
  mbedtls_ctr_drbg_init(&data->ctr_drbg);

  int ret = 0;
  mbedtls_entropy_init(&data->entropy);
  if ((ret = mbedtls_ctr_drbg_seed(&data->ctr_drbg, mbedtls_entropy_func,
                                   &data->entropy, NULL, 0)) != 0) {
    ESP_LOGE("GEN_MBEDTLS", "ERROR mbedtls_ctr_drbg_seed returned %d\n", ret);
    return;
  }

  if ((ret = mbedtls_ssl_config_defaults(&data->conf, MBEDTLS_SSL_IS_CLIENT,
                                         MBEDTLS_SSL_TRANSPORT_DATAGRAM,
                                         MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
    ESP_LOGE("GEN_MBEDTLS", "ERROR mbedtls_ssl_config_defaults returned %d\n\n",
             ret);

    return;
  }

  mbedtls_ssl_conf_psk(
      &data->conf, reinterpret_cast<const unsigned char *>(password.data()),
      password.size(), reinterpret_cast<const unsigned char *>(username.data()),
      username.size());

  mbedtls_ssl_conf_authmode(&data->conf, MBEDTLS_SSL_VERIFY_NONE);
  mbedtls_ssl_conf_rng(&data->conf, mbedtls_ctr_drbg_random, &data->ctr_drbg);
  // mbedtls_ssl_conf_dbg(&data->conf, debug_mbedtls, this);

  ESP_LOGI("GEN_MBEDTLS", "Mbedtls setup finished!");
}

extern "C" void app_main(void) {
  WifiHandlerParameter wifi_param{
      .ssid = "Mecanum",
      .password = "12345678",
      .ip = "192.168.1.1",
      .netmask = "255.255.255.0",
      .gateway = "192.168.1.1",
  };
  WifiHandler wifi_handler(wifi_param);
  UDPHandler udphandler(5555);

  mbedtls_config_data mbedtls_cfg{};

  gen_mbedtls_config(&mbedtls_cfg, "testudp", "testudp");

  Tunnel *testune =
      new Tunnel("192.168.1.2", 5555, udphandler, mbedtls_cfg.conf);

  vTaskDelay(pdMS_TO_TICKS(15000));
  while (testune->handshake()) {
    ESP_LOGI("TESTUNE", "Handshaking");
  }

  while (true) {
    vTaskDelay(10);
  }
}
