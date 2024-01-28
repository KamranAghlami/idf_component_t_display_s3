#pragma once

#include <memory>

namespace hardware
{
    struct wifi_implementation;

    class wifi
    {
    public:
        static wifi &get() { return s_instance; };

        ~wifi();

        wifi(const wifi &) = delete;
        wifi(wifi &&) = delete;
        wifi &operator=(const wifi &) = delete;
        wifi &operator=(wifi &&) = delete;

        void init();

    private:
        static wifi s_instance;

        std::unique_ptr<wifi_implementation> mp_implementation;

        wifi();
    };
}