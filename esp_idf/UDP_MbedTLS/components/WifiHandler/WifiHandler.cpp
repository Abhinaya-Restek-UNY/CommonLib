#include "WifiHandler.hpp"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wifi_types_generic.h"
#include "nvs_flash.h"
#include <cstring>
#define WIFI_TAG "WIFI"

static const char *mac2str(const uint8_t mac[6]) {
  static char buf[18];

  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1],
           mac[2], mac[3], mac[4], mac[5]);

  return buf;
}

WifiHandler::WifiHandler(WifiHandlerParameter &parameter) {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }

  ESP_ERROR_CHECK(ret);

  ESP_ERROR_CHECK(esp_netif_init());

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  ESP_ERROR_CHECK(esp_event_loop_create_default());
  ESP_ERROR_CHECK(esp_event_handler_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &this->esp_wifi_event_forward, this));

  this->setup_interface(parameter.ip, parameter.gateway, parameter.netmask);
  this->setup_ap(parameter.ssid, parameter.password);
}

void WifiHandler::esp_wifi_event_forward(void *this_ref,
                                         esp_event_base_t event_base,
                                         int32_t event_id, void *event_data) {
  ((WifiHandler *)this_ref)
      ->wifi_event_handler(event_base, event_id, event_data);
}

void WifiHandler::wifi_event_handler(esp_event_base_t event_base,
                                     int32_t event_id, void *event_data) {
  switch (event_id) {
  case WIFI_EVENT_AP_START:
    ESP_LOGI(WIFI_TAG, "AP Started.");
    break;
  case WIFI_EVENT_AP_STADISCONNECTED:
    wifi_event_ap_stadisconnected_t *event =
        (wifi_event_ap_stadisconnected_t *)event_data;
    ESP_LOGI(WIFI_TAG, "station %s leave, AID=%d, reason=%d",
             mac2str(event->mac), event->aid, event->reason);
    break;
  }
};

void WifiHandler::setup_interface(std::string ip_address, std::string gateway,
                                  std::string netmask) {

  this->interface = esp_netif_create_default_wifi_ap();

  ESP_ERROR_CHECK(esp_netif_dhcps_stop(this->interface));
  esp_netif_ip_info_t ip = {};
  ip.ip.addr = esp_ip4addr_aton(ip_address.c_str());
  ip.gw.addr = esp_ip4addr_aton(gateway.c_str());
  ip.netmask.addr = esp_ip4addr_aton(netmask.c_str());

  ESP_ERROR_CHECK(esp_netif_set_ip_info(this->interface, &ip));

  /* restart DHCP server */
  ESP_ERROR_CHECK(esp_netif_dhcps_start(this->interface));
}

void WifiHandler::setup_ap(std::string ssid, std::string password) {

  uint8_t password_length = static_cast<uint8_t>(password.length());
  uint8_t ssid_length = static_cast<uint8_t>(ssid.length());

  if (password_length > 64) {
    ESP_LOGE(WIFI_TAG, "WiFi error password(%d) need to be less than 64",
             password_length);
    return;
  }
  if (ssid_length > 32) {

    ESP_LOGE(WIFI_TAG, "WiFi error ssid(%d) need to be less than 64",
             ssid_length);
  }
  wifi_config_t wifi_config = {
      .ap =
          {
              .ssid_len = ssid_length,
              .channel = 10,
              .authmode = WIFI_AUTH_WPA2_PSK,
              .max_connection = 2,
              .pmf_cfg =
                  {
                      .capable = true,
                      .required = false,
                  },
          },
  };

  memcpy(wifi_config.ap.ssid, ssid.c_str(), ssid_length);
  memcpy(wifi_config.ap.password, password.c_str(), password_length);

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));

  ESP_ERROR_CHECK(esp_wifi_start());
}
