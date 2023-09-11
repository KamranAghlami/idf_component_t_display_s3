#include "hardware/battery.h"

#include <driver/gpio.h>

static const gpio_num_t PIN_BATTERY = GPIO_NUM_4;

namespace hardware
{
    battery battery::s_instance;

    battery::battery()
    {
        // pinMode(PIN_BATTERY, INPUT);
    }

    battery::~battery()
    {
    }

    uint32_t battery::voltage_level()
    {
        return 0; // analogReadMilliVolts(PIN_BATTERY) * 2;
    }
}
