#include "hardware/wifi.h"

#include <cstring>

#include <esp_log.h>
#include <esp_err.h>
#include <esp_wifi.h>
#include <esp_mac.h>
#include <nvs_flash.h>

namespace hardware
{
    enum class wifi_mode : uint8_t
    {
        ACCESS_POINT,
        STATION,
    };

    constexpr const char *TAG = "wifi";
    constexpr const char *AP_DEFAULT_SSID = "RCLink";
    constexpr const char *AP_DEFAULT_PASS = "0123456789";
    constexpr uint8_t AP_CHAN = 1;
    constexpr uint8_t AP_MAX_CONN = 3;

    struct wifi_implementation
    {
        ~wifi_implementation()
        {
            if (ssid)
                delete[] ssid;

            if (password)
                delete[] password;
        }

        void load_config()
        {
            esp_err_t error = ESP_OK;

            error = nvs_get_u8(nvs_handle, "mode", reinterpret_cast<uint8_t *>(&mode));

            assert(error == ESP_OK || error == ESP_ERR_NVS_NOT_FOUND);

            if (error == ESP_ERR_NVS_NOT_FOUND)
                mode = wifi_mode::ACCESS_POINT;

            size_t size = 0;

            error = nvs_get_str(nvs_handle, "ssid", nullptr, &size);

            assert(error == ESP_OK || error == ESP_ERR_NVS_NOT_FOUND);

            if (error == ESP_OK)
            {
                ssid = new char[size];

                ESP_ERROR_CHECK(nvs_get_str(nvs_handle, "ssid", ssid, &size));
            }
            else
            {
                ssid = new char[strlen(AP_DEFAULT_SSID) + 1];

                strcpy(ssid, AP_DEFAULT_SSID);
            }

            size = 0;

            error = nvs_get_str(nvs_handle, "password", nullptr, &size);

            assert(error == ESP_OK || error == ESP_ERR_NVS_NOT_FOUND);

            if (error == ESP_OK)
            {
                password = new char[size];

                ESP_ERROR_CHECK(nvs_get_str(nvs_handle, "password", password, &size));
            }
            else
            {
                password = new char[strlen(AP_DEFAULT_PASS) + 1];

                strcpy(password, AP_DEFAULT_PASS);
            }
        }

        nvs_handle_t nvs_handle = 0;
        wifi_mode mode = wifi_mode::ACCESS_POINT;
        char *ssid = nullptr;
        char *password = nullptr;
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
        ESP_ERROR_CHECK(nvs_open(TAG, NVS_READWRITE, &mp_implementation->nvs_handle));

        mp_implementation->load_config();

        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());

        if (mp_implementation->mode == wifi_mode::ACCESS_POINT)
            mp_implementation->network_interface = esp_netif_create_default_wifi_ap();
        else
            mp_implementation->network_interface = esp_netif_create_default_wifi_sta();

        const wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();

        ESP_ERROR_CHECK(esp_wifi_init(&init_config));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &ip_event_handler, NULL, NULL));

        wifi_config_t wifi_config = {};

        if (mp_implementation->mode == wifi_mode::ACCESS_POINT)
        {
            strcpy(reinterpret_cast<char *>(wifi_config.ap.ssid), mp_implementation->ssid);
            wifi_config.ap.ssid_len = strlen(mp_implementation->ssid);
            wifi_config.ap.channel = AP_CHAN;
            wifi_config.ap.max_connection = AP_MAX_CONN;

            if (strlen(mp_implementation->password))
            {
                strcpy(reinterpret_cast<char *>(wifi_config.ap.password), mp_implementation->password);

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
            strcpy(reinterpret_cast<char *>(wifi_config.sta.ssid), mp_implementation->ssid);
            strcpy(reinterpret_cast<char *>(wifi_config.sta.password), mp_implementation->password);

            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        }

        ESP_ERROR_CHECK(esp_wifi_start());
    }

    wifi::~wifi()
    {
        ESP_ERROR_CHECK(esp_wifi_stop());
        esp_netif_destroy_default_wifi(mp_implementation->network_interface);
        ESP_ERROR_CHECK(esp_event_loop_delete_default());
        ESP_ERROR_CHECK(esp_netif_deinit());
        nvs_close(mp_implementation->nvs_handle);
    }
}
