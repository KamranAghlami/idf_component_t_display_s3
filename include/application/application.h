#pragma once

#include <cstdint>
#include <memory>

#include <lvgl/lvgl.h>

class application
{
public:
    application();
    virtual ~application();

    application(const application &) = delete;
    application(application &&) = delete;
    application &operator=(const application &) = delete;
    application &operator=(application &&) = delete;

    bool is_running() { return m_is_running; };

protected:
    virtual void on_create() = 0;
    virtual void on_update(float timestep) = 0;

    bool m_is_running;

private:
    static application *s_instance;

    lv_disp_draw_buf_t m_draw_buf;
    lv_disp_drv_t m_disp_drv;
    lv_indev_drv_t m_indev_drv;

    int64_t m_previous_timestamp = 0;
};

std::unique_ptr<application> create_application();