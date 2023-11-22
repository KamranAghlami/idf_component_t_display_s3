#include "application/msc_application.h"

#include <esp_err.h>
#include <tusb_msc_storage.h>
#include <tinyusb.h>

#include "hardware/storage.h"
#include "hardware/display.h"

#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_MSC_DESC_LEN)

enum
{
    ITF_NUM_MSC = 0,
    ITF_NUM_TOTAL
};

enum
{
    EDPT_CTRL_OUT = 0x00,
    EDPT_CTRL_IN = 0x80,

    EDPT_MSC_OUT = 0x01,
    EDPT_MSC_IN = 0x81,
};

msc_application::msc_application() : m_wl_handle(WL_INVALID_HANDLE),
                                     m_width(hardware::display::get().width()),
                                     m_height(hardware::display::get().height()),
                                     m_group(lv_group_create()),
                                     m_screen(lv_scr_act())
{
    hardware::storage::unmount(LV_FS_POSIX_PATH);

    const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "storage");

    if (!partition)
    {
        m_is_running = false;

        return;
    }

    ESP_ERROR_CHECK(wl_mount(partition, &m_wl_handle));

    const tinyusb_msc_spiflash_config_t spiflash_config = {
        .wl_handle = m_wl_handle,
        .callback_mount_changed = nullptr,    /*!< Pointer to the function callback that will be delivered AFTER mount/unmount operation is successfully finished */
        .callback_premount_changed = nullptr, /*!< Pointer to the function callback that will be delivered BEFORE mount/unmount operation is started */
        .mount_config = {},
    };

    ESP_ERROR_CHECK(tinyusb_msc_storage_init_spiflash(&spiflash_config));
    ESP_ERROR_CHECK(tinyusb_msc_storage_mount(LV_FS_POSIX_PATH));

    static tusb_desc_device_t device_descriptor = {
        .bLength = sizeof(device_descriptor),
        .bDescriptorType = TUSB_DESC_DEVICE,
        .bcdUSB = 0x0200,
        .bDeviceClass = TUSB_CLASS_MISC,
        .bDeviceSubClass = MISC_SUBCLASS_COMMON,
        .bDeviceProtocol = MISC_PROTOCOL_IAD,
        .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
        .idVendor = 0x303A, // This is Espressif VID. This needs to be changed according to Users / Customers
        .idProduct = 0x4002,
        .bcdDevice = 0x100,
        .iManufacturer = 0x01,
        .iProduct = 0x02,
        .iSerialNumber = 0x03,
        .bNumConfigurations = 0x01,
    };

    static char const *string_descriptor[] = {
        (const char[]){0x09, 0x04}, // 0: is supported language is English (0x0409)
        "LILYGO",                   // 1: Manufacturer
        "T-Display-S3",             // 2: Product
        "0123456789",               // 3: Serials
        "Espressif MSC Device",     // 4. MSC
    };

    static uint8_t const configuration_descriptor[] = {
        // Config number, interface count, string index, total length, attribute, power in mA
        TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

        // Interface number, string index, EP Out & EP In address, EP size
        TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 0, EDPT_MSC_OUT, EDPT_MSC_IN, TUD_OPT_HIGH_SPEED ? 512 : 64),
    };

    const tinyusb_config_t tinyusb_config = {
        .device_descriptor = &device_descriptor,
        .string_descriptor = string_descriptor,
        .string_descriptor_count = sizeof(string_descriptor) / sizeof(string_descriptor[0]),
        .external_phy = false,
        .configuration_descriptor = configuration_descriptor,
        .self_powered = false,
        .vbus_monitor_io = 0,
    };

    ESP_ERROR_CHECK(tinyusb_driver_install(&tinyusb_config));
    ESP_ERROR_CHECK(tinyusb_msc_storage_unmount());

    lv_indev_t *indev = nullptr;

    while ((indev = lv_indev_get_next(indev)))
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_KEYPAD)
            lv_indev_set_group(indev, m_group);

    lv_group_add_obj(m_group, m_screen);

    auto on_key = [](lv_event_t *e)
    {
        auto app = static_cast<msc_application *>(lv_event_get_user_data(e));
        auto key = lv_event_get_key(e);

        switch (key)
        {
        case LV_KEY_UP:
            break;
        case LV_KEY_DOWN:
            break;
        case LV_KEY_ENTER:
            app->m_is_running = false;
            break;
        default:
            break;
        }
    };

    lv_obj_add_event_cb(m_screen, on_key, LV_EVENT_KEY, this);
}

msc_application::~msc_application()
{
    lv_group_del(m_group);

    ESP_ERROR_CHECK(tinyusb_msc_storage_mount(LV_FS_POSIX_PATH));

    tinyusb_msc_storage_deinit();

    ESP_ERROR_CHECK(wl_unmount(m_wl_handle));
}

void msc_application::on_create()
{
    lv_obj_clear_flag(m_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(m_screen, lv_color_black(), LV_STATE_DEFAULT);

    m_icon = lv_img_create(m_screen);

    LV_IMG_DECLARE(msc_icon);

    lv_img_set_src(m_icon, &msc_icon);
    lv_obj_center(m_icon);
}

void msc_application::on_update(float timestep)
{
}
