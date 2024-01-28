#pragma once

#include <memory>

namespace hardware
{
    struct wifi_implementation;

    class wifi
    {
    public:
        static wifi &get()
        {
            if (sp_instance)
                return *sp_instance;

            sp_instance = new wifi();

            return *sp_instance;
        };

        ~wifi();

        wifi(const wifi &) = delete;
        wifi(wifi &&) = delete;
        wifi &operator=(const wifi &) = delete;
        wifi &operator=(wifi &&) = delete;

        void init();

    private:
        static wifi *sp_instance;

        std::unique_ptr<wifi_implementation> mp_implementation;

        wifi();
    };
}