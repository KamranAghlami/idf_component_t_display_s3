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

    const uint16_t m_width;
    const uint16_t m_height;

    lv_group_t *m_group;
    lv_obj_t *m_screen;
};