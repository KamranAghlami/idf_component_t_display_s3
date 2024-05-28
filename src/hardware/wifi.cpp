#include "hardware/wifi.h"

#include <cstring>

#include <esp_log.h>
#include <esp_err.h>
#include <esp_wifi.h>
#include <esp_mac.h>

namespace hardware
{
    enum class wifi_mode
    {
        ACCESS_POINT,
        STATION,
    };

    constexpr const char *TAG = "wifi";
    constexpr const char *WIFI_SSID = "RCLink";
    constexpr const char *WIFI_PASS = "0123456789";
    constexpr wifi_mode MODE = wifi_mode::ACCESS_POINT;
    constexpr uint8_t WIFI_CHAN = 1;
    constexpr uint8_t MAX_CONN = 2;

    struct wifi_implementation
    {
        esp_netif_t *network_interface = nullptr;
    };

    wifi *wifi::sp_instance = nullptr;

    static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                   int32_t event_id, void *event_data)
    {
        switch (event_id)
        {
        case WIFI_EVENT_AP_STACONNECTED:
        {
            auto *event = static_cast<wifi_event_ap_staconnected_t *>(event_data);

            ESP_LOGI(TAG, "new connection from " MACSTR, MAC2STR(event->mac));

            break;
        }

        case WIFI_EVENT_AP_STADISCONNECTED:
        {
            auto *event = static_cast<wifi_event_ap_stadisconnected_t *>(event_data);

            ESP_LOGI(TAG, "lost connection to " MACSTR, MAC2STR(event->mac));

            break;
        }

        case WIFI_EVENT_STA_CONNECTED:
        {
            auto *event = static_cast<wifi_event_sta_connected_t *>(event_data);

            ESP_LOGI(TAG, "connected to %s", event->ssid);

            break;
        }

        case WIFI_EVENT_STA_DISCONNECTED:
        {
            auto *event = static_cast<wifi_event_sta_disconnected_t *>(event_data);

            ESP_LOGI(TAG, "disconnected from %s, retrying...", event->ssid);

            esp_wifi_connect();

            break;
        }

        case WIFI_EVENT_STA_START:
        {
            esp_wifi_connect();

            break;
        }

        default:
            break;
        }
    }

    static void ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
    {
        switch (event_id)
        {
        case IP_EVENT_AP_STAIPASSIGNED:
        {
            auto *event = static_cast<ip_event_ap_staipassigned_t *>(event_data);

            ESP_LOGI(TAG, "client ip: " IPSTR, IP2STR(&event->ip));

            break;
        }

        case IP_EVENT_STA_GOT_IP:
        {
            auto *event = static_cast<ip_event_got_ip_t *>(event_data);

            ESP_LOGI(TAG, "assigned ip: " IPSTR, IP2STR(&event->ip_info.ip));

            break;
        }

        default:
            break;
        }
    }

    wifi::wifi() : mp_implementation(std::make_unique<wifi_implementation>())
    {
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());

        if (MODE == wifi_mode::ACCESS_POINT)
            mp_implementation->network_interface = esp_netif_create_default_wifi_ap();
        else
            mp_implementation->network_interface = esp_netif_create_default_wifi_sta();

        const wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();

        ESP_ERROR_CHECK(esp_wifi_init(&init_config));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &ip_event_handler, NULL, NULL));

        wifi_config_t wifi_config = {};

        if (MODE == wifi_mode::ACCESS_POINT)
        {
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
        }
        else
        {
            strcpy(reinterpret_cast<char *>(wifi_config.sta.ssid), WIFI_SSID);
            strcpy(reinterpret_cast<char *>(wifi_config.sta.password), WIFI_PASS);

            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        }

        ESP_ERROR_CHECK(esp_wifi_start());
    }

    wifi::~wifi()
    {
        esp_netif_destroy_default_wifi(mp_implementation->network_interface);
        ESP_ERROR_CHECK(esp_event_loop_delete_default());
        ESP_ERROR_CHECK(esp_netif_deinit());
    }
}
