#include "hardware/wifi.h"

#include <cstring>

#include <esp_log.h>
#include <esp_err.h>
#include <esp_wifi.h>
#include <esp_mac.h>

namespace hardware
{
    static const char *TAG = "wifi softAP";
    static const char *WIFI_SSID = "T-Display-S3";
    static const char *WIFI_PASS = "0123456789";
    static const uint8_t WIFI_CHAN = 1;
    static const uint8_t MAX_CONN = 2;

    struct wifi_implementation
    {
        esp_netif_t *network_interface = nullptr;
    };

    wifi *wifi::sp_instance = nullptr;

    static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                   int32_t event_id, void *event_data)
    {
        if (event_id == WIFI_EVENT_AP_STACONNECTED)
        {
            wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;

            ESP_LOGI(TAG, "station " MACSTR " join, AID=%d", MAC2STR(event->mac), event->aid);
        }
        else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
        {
            wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;

            ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d", MAC2STR(event->mac), event->aid);
        }
    }

    wifi::wifi() : mp_implementation(std::make_unique<wifi_implementation>())
    {
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());

        mp_implementation->network_interface = esp_netif_create_default_wifi_ap();

        const wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();

        ESP_ERROR_CHECK(esp_wifi_init(&init_config));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

        wifi_config_t wifi_config = {};

        strcpy(reinterpret_cast<char *>(wifi_config.ap.ssid), WIFI_SSID);
        wifi_config.ap.ssid_len = strlen(WIFI_SSID);
        wifi_config.ap.channel = WIFI_CHAN;
        wifi_config.ap.max_connection = MAX_CONN;

        if (strlen(WIFI_PASS))
        {
            strcpy(reinterpret_cast<char *>(wifi_config.ap.password), WIFI_PASS);

#ifdef CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT
            wifi_config.ap.authmode = WIFI_AUTH_WPA3_PSK;
            wifi_config.ap.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
#else
            wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
#endif
        }
        else
            wifi_config.ap.authmode = WIFI_AUTH_OPEN;

        wifi_config.ap.pmf_cfg = {
            .capable = true,
            .required = true,
        };

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());

        ESP_LOGI(TAG, "started! SSID: %s, password: %s, channel: %d",
                 WIFI_SSID, WIFI_PASS, WIFI_CHAN);
    }

    wifi::~wifi()
    {
        esp_netif_destroy_default_wifi(mp_implementation->network_interface);
    }

    void wifi::init()
    {
        ESP_LOGI(TAG, "init");
    }
}