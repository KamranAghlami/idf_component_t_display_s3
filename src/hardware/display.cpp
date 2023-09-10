#include "hardware/display.h"

#include <driver/gpio.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>

#define PIN_LCD_BACKLIGHT GPIO_NUM_38
#define PIN_LCD_CS GPIO_NUM_6
#define PIN_LCD_D0 GPIO_NUM_39
#define PIN_LCD_D1 GPIO_NUM_40
#define PIN_LCD_D2 GPIO_NUM_41
#define PIN_LCD_D3 GPIO_NUM_42
#define PIN_LCD_D4 GPIO_NUM_45
#define PIN_LCD_D5 GPIO_NUM_46
#define PIN_LCD_D6 GPIO_NUM_47
#define PIN_LCD_D7 GPIO_NUM_48
#define PIN_LCD_DC GPIO_NUM_7
#define PIN_LCD_POWER GPIO_NUM_15
#define PIN_LCD_RD GPIO_NUM_9
#define PIN_LCD_RES GPIO_NUM_5
#define PIN_LCD_WR GPIO_NUM_8
#define LCD_PIXELS_WIDTH 320
#define LCD_PIXELS_HEIGHT 170

namespace hardware
{
    struct display_implementation
    {
        esp_lcd_i80_bus_handle_t bus_handle = nullptr;
        esp_lcd_panel_io_handle_t io_handle = nullptr;
        esp_lcd_panel_handle_t panel_handle = nullptr;
    };

    display display::s_instance;

    display::display() : mp_implementation(static_cast<void *>(new display_implementation()))
    {
        gpio_config_t gpio_cfg = {};

        gpio_cfg.pin_bit_mask = (1ULL << PIN_LCD_RD) | (1ULL << PIN_LCD_POWER) | (1ULL << PIN_LCD_BACKLIGHT);
        gpio_cfg.mode = GPIO_MODE_OUTPUT;

        ESP_ERROR_CHECK(gpio_config(&gpio_cfg));
        ESP_ERROR_CHECK(gpio_set_level(PIN_LCD_RD, 1));
        ESP_ERROR_CHECK(gpio_set_level(PIN_LCD_POWER, 1));

        set_backlight(brightness_level::min);

        auto implementation = static_cast<display_implementation *>(mp_implementation);

        esp_lcd_i80_bus_config_t bus_config = {
            .dc_gpio_num = PIN_LCD_DC,
            .wr_gpio_num = PIN_LCD_WR,
            .clk_src = LCD_CLK_SRC_DEFAULT,
            .data_gpio_nums =
                {
                    PIN_LCD_D0,
                    PIN_LCD_D1,
                    PIN_LCD_D2,
                    PIN_LCD_D3,
                    PIN_LCD_D4,
                    PIN_LCD_D5,
                    PIN_LCD_D6,
                    PIN_LCD_D7,
                },
            .bus_width = 8,
            .max_transfer_bytes = LCD_PIXELS_WIDTH * LCD_PIXELS_HEIGHT * sizeof(uint16_t),
            .psram_trans_align = 4,
            .sram_trans_align = 4,
        };

        ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_config, &(implementation->bus_handle)));

        auto on_transfer_done = [](esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
        {
            auto disp = static_cast<display *>(user_ctx);

            disp->m_on_transfer_done_callback(disp->m_on_transfer_done_user_data);

            return false;
        };

        esp_lcd_panel_io_i80_config_t io_config = {
            .cs_gpio_num = PIN_LCD_CS,
            .pclk_hz = 10 * 1000 * 1000,
            .trans_queue_depth = 20,
            .on_color_trans_done = on_transfer_done,
            .user_ctx = this,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
            .dc_levels = {
                .dc_idle_level = 0,
                .dc_cmd_level = 0,
                .dc_dummy_level = 0,
                .dc_data_level = 1,
            },
        };

        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(implementation->bus_handle, &io_config, &(implementation->io_handle)));

        esp_lcd_panel_dev_config_t device_config = {
            .reset_gpio_num = PIN_LCD_RES,
            // .color_space = ESP_LCD_COLOR_SPACE_RGB,
            .bits_per_pixel = 16,
        };

        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(implementation->io_handle, &device_config, &(implementation->panel_handle)));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(implementation->panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_init(implementation->panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(implementation->panel_handle, true));
        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(implementation->panel_handle, true));
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(implementation->panel_handle, false, true));
        ESP_ERROR_CHECK(esp_lcd_panel_set_gap(implementation->panel_handle, 0, 35));
    }

    display::~display()
    {
        ESP_ERROR_CHECK(gpio_reset_pin(PIN_LCD_BACKLIGHT));

        auto implementation = static_cast<display_implementation *>(mp_implementation);

        ESP_ERROR_CHECK(esp_lcd_panel_del(implementation->panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_io_del(implementation->io_handle));
        ESP_ERROR_CHECK(esp_lcd_del_i80_bus(implementation->bus_handle));

        ESP_ERROR_CHECK(gpio_reset_pin(PIN_LCD_POWER));
        ESP_ERROR_CHECK(gpio_reset_pin(PIN_LCD_RD));

        delete implementation;
    }

    uint16_t display::width()
    {
        return LCD_PIXELS_WIDTH;
    }

    uint16_t display::height()
    {
        return LCD_PIXELS_HEIGHT;
    }

    void display::set_backlight(brightness_level level)
    {
        switch (level)
        {
        case brightness_level::min:
            ESP_ERROR_CHECK(gpio_set_level(PIN_LCD_BACKLIGHT, 0));
            break;

        case brightness_level::max:
            ESP_ERROR_CHECK(gpio_set_level(PIN_LCD_BACKLIGHT, 1));
            break;

        default:
            break;
        }
    }

    void display::set_transfer_done_callback(transfer_done_callback_t on_transfer_done, void *user_data)
    {
        m_on_transfer_done_callback = on_transfer_done;
        m_on_transfer_done_user_data = user_data;
    }

    void display::set_bitmap(uint16_t x1, uint16_t x2, uint16_t y1, uint16_t y2, uint16_t *data)
    {
        esp_lcd_panel_draw_bitmap(static_cast<display_implementation *>(mp_implementation)->panel_handle, x1, y1, x2 + 1, y2 + 1, data);
    }
}
