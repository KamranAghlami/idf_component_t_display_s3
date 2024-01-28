#pragma once

namespace hardware
{
    class wifi
    {
    public:
        static wifi &get() { return s_instance; };

        ~wifi();

        wifi(const wifi &) = delete;
        wifi(wifi &&) = delete;
        wifi &operator=(const wifi &) = delete;
        wifi &operator=(wifi &&) = delete;

    private:
        static wifi s_instance;

        wifi();
    };
}