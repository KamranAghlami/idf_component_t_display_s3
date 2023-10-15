#pragma once

#include <wear_levelling.h>

#include "application/application.h"

class msc_application : public application
{
public:
    msc_application();
    ~msc_application();

    void on_create() override;
    void on_update(float timestep) override;

private:
    wl_handle_t m_wl_handle;
};