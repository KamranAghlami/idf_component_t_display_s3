#include "hardware/battery.h"

// #include <Arduino.h>

#include "hardware/config.h"

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
