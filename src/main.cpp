#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "application/msc_application.h"

extern "C" void app_main(void)
{
    const gpio_config_t gpio_cfg = {
        .pin_bit_mask = (1ULL << GPIO_NUM_14),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&gpio_cfg));

    std::unique_ptr<application> application = nullptr;

    if (!gpio_get_level(GPIO_NUM_14))
        application = std::make_unique<msc_application>();
    else
        application = create_application();

    while (application->is_running())
        vTaskDelay(pdMS_TO_TICKS(lv_timer_handler()));

    esp_restart();
}