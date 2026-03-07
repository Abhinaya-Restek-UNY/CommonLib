#include "esp_event_base.h"
#include "esp_netif_types.h"
#include <string>

struct WifiHandlerParameter {
  std::string ssid;
  std::string password;
  std::string ip;
  std::string netmask;
  std::string gateway;
};
class WifiHandler {
public:
  WifiHandler(WifiHandlerParameter &parameter);

private:
  uint8_t ssid[32] = {};
  uint8_t password[64] = {};
  static void esp_wifi_event_forward(void *this_ref,
                                     esp_event_base_t event_base,
                                     int32_t event_id, void *event_data);
  void wifi_event_handler(esp_event_base_t event_base, int32_t event_id,
                          void *event_data);

  esp_netif_t *interface;

  void setup_interface(std::string ip, std::string gateway,
                       std::string netmask);
  void setup_ap(std::string ssid, std::string password);
};
