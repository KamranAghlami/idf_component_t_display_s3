#include "hardware/wifi.h"

#include <cstring>

#include <esp_log.h>
#include <esp_err.h>
#include <esp_wifi.h>
#include <esp_mac.h>
#include <nvs_flash.h>

namespace hardware
{
    constexpr const char *TAG = "wifi";
    constexpr const char *AP_DEFAULT_SSID = "RCLink";
    constexpr const char *AP_DEFAULT_PASS = "0123456789";
    constexpr uint8_t AP_CHAN = 1;
    constexpr uint8_t AP_MAX_CONN = 3;

    struct wifi_implementation
    {
        ~wifi_implementation()
        {
            if (m_ssid)
                delete[] m_ssid;

            if (m_password)
                delete[] m_password;
        }

        void load_config()
        {
            esp_err_t error = ESP_OK;

            error = nvs_get_u8(m_nvs_handle, "mode", reinterpret_cast<uint8_t *>(&m_mode));

            assert(error == ESP_OK || error == ESP_ERR_NVS_NOT_FOUND);

            if (error == ESP_ERR_NVS_NOT_FOUND)
                m_mode = wifi::mode::ACCESS_POINT;

            size_t size = 0;

            error = nvs_get_str(m_nvs_handle, "ssid", nullptr, &size);

            assert(error == ESP_OK || error == ESP_ERR_NVS_NOT_FOUND);

            if (error == ESP_OK)
            {
                m_ssid = new char[size];

                ESP_ERROR_CHECK(nvs_get_str(m_nvs_handle, "ssid", m_ssid, &size));
            }
            else
            {
                m_ssid = new char[strlen(AP_DEFAULT_SSID) + 1];

                strcpy(m_ssid, AP_DEFAULT_SSID);
            }

            size = 0;

            error = nvs_get_str(m_nvs_handle, "password", nullptr, &size);

            assert(error == ESP_OK || error == ESP_ERR_NVS_NOT_FOUND);

            if (error == ESP_OK)
            {
                m_password = new char[size];

                ESP_ERROR_CHECK(nvs_get_str(m_nvs_handle, "password", m_password, &size));
            }
            else
            {
                if (AP_DEFAULT_PASS)
                {
                    m_password = new char[strlen(AP_DEFAULT_PASS) + 1];

                    strcpy(m_password, AP_DEFAULT_PASS);
                }
                else
                    m_password = nullptr;
            }
        }

        void save_config()
        {
            if (m_config_changed)
            {
                m_config_changed = false;

                ESP_ERROR_CHECK(nvs_set_u8(m_nvs_handle, "mode", static_cast<uint8_t>(m_mode)));
                ESP_ERROR_CHECK(nvs_set_str(m_nvs_handle, "ssid", m_ssid));

                if (m_password)
                    ESP_ERROR_CHECK(nvs_set_str(m_nvs_handle, "password", m_password));
                else
                    ESP_ERROR_CHECK(nvs_erase_key(m_nvs_handle, "password"));
            }
        }

        void set_mode(wifi::mode m)
        {
            m_mode = m;
            m_config_changed = true;
        }

        void set_ssid(const char *ssid)
        {
            assert(ssid);

            if (m_ssid)
                delete[] m_ssid;

            m_ssid = new char[strlen(ssid) + 1];

            strcpy(m_ssid, ssid);

            m_config_changed = true;
        }

        void set_password(const char *password)
        {
            if (m_password)
                delete[] m_password;

            m_password = nullptr;

            if (password)
            {
                m_password = new char[strlen(password) + 1];

                strcpy(m_password, password);
            }

            m_config_changed = true;
        }

        nvs_handle_t m_nvs_handle = 0;
        wifi::mode m_mode = wifi::mode::ACCESS_POINT;
        char *m_ssid = nullptr;
        char *m_password = nullptr;
        esp_netif_t *m_network_interface = nullptr;
        uint8_t m_try_count = 0;
        bool m_config_changed = false;
    };

    wifi *wifi::sp_instance = nullptr;

    static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                   int32_t event_id, void *event_data)
    {
        auto impl = static_cast<wifi_implementation *>(arg);

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

            impl->save_config();
            impl->m_try_count = 0;

            break;
        }

        case WIFI_EVENT_STA_DISCONNECTED:
        {
            auto *event = static_cast<wifi_event_sta_disconnected_t *>(event_data);

            if (impl->m_try_count == 3)
            {
                ESP_LOGE(TAG, "failed to connect to %s, switching to access point mode", event->ssid);

                impl->set_mode(wifi::mode::ACCESS_POINT);
                impl->set_ssid(AP_DEFAULT_SSID);
                impl->set_password(AP_DEFAULT_PASS);
                impl->save_config();
                impl->m_try_count = 0;

                esp_restart(); // shouldn't have to do this.

                return;
            }

            impl->m_try_count++;

            ESP_LOGI(TAG, "disconnected from %s, retrying... (%hhu/3)", event->ssid, impl->m_try_count);

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
        ESP_ERROR_CHECK(nvs_open(TAG, NVS_READWRITE, &mp_implementation->m_nvs_handle));

        mp_implementation->load_config();

        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());

        if (mp_implementation->m_mode == mode::ACCESS_POINT)
            mp_implementation->m_network_interface = esp_netif_create_default_wifi_ap();
        else
            mp_implementation->m_network_interface = esp_netif_create_default_wifi_sta();

        const wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();

        ESP_ERROR_CHECK(esp_wifi_init(&init_config));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, mp_implementation.get(), nullptr));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &ip_event_handler, mp_implementation.get(), nullptr));

        wifi_config_t wifi_config = {};

        if (mp_implementation->m_mode == mode::ACCESS_POINT)
        {
            strcpy(reinterpret_cast<char *>(wifi_config.ap.ssid), mp_implementation->m_ssid);
            wifi_config.ap.ssid_len = strlen(mp_implementation->m_ssid);
            wifi_config.ap.channel = AP_CHAN;
            wifi_config.ap.max_connection = AP_MAX_CONN;

            if (strlen(mp_implementation->m_password))
            {
                strcpy(reinterpret_cast<char *>(wifi_config.ap.password), mp_implementation->m_password);

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
            strcpy(reinterpret_cast<char *>(wifi_config.sta.ssid), mp_implementation->m_ssid);
            strcpy(reinterpret_cast<char *>(wifi_config.sta.password), mp_implementation->m_password);

            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        }

        ESP_ERROR_CHECK(esp_wifi_start());
    }

    wifi::~wifi()
    {
        ESP_ERROR_CHECK(esp_wifi_stop());
        esp_netif_destroy_default_wifi(mp_implementation->m_network_interface);
        ESP_ERROR_CHECK(esp_event_loop_delete_default());
        ESP_ERROR_CHECK(esp_netif_deinit());
        nvs_close(mp_implementation->m_nvs_handle);
    }

    void wifi::set_mode(mode m)
    {
        mp_implementation->set_mode(m);
    }

    wifi::mode wifi::get_mode()
    {
        return mp_implementation->m_mode;
    }

    void wifi::set_ssid(const char *ssid)
    {
        mp_implementation->set_ssid(ssid);
    }

    const char *wifi::get_ssid()
    {
        return mp_implementation->m_ssid;
    }

    void wifi::set_password(const char *password)
    {
        mp_implementation->set_password(password);
    }

    const char *wifi::get_password()
    {
        return mp_implementation->m_password;
    }
}
