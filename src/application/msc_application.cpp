#include "application/msc_application.h"

#include <esp_err.h>
#include <tusb_msc_storage.h>
#include <tinyusb.h>

#include "hardware/storage.h"

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

msc_application::msc_application() : m_wl_handle(WL_INVALID_HANDLE)
{
    hardware::storage::unmount(LV_FS_POSIX_PATH);

    const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "storage");

    if (!partition)
    {
        m_is_running = false;

        return;
    }

    ESP_ERROR_CHECK(wl_mount(partition, &m_wl_handle));

    const tinyusb_msc_spiflash_config_t config_spi = {
        .wl_handle = m_wl_handle,
        .callback_mount_changed = nullptr,    /*!< Pointer to the function callback that will be delivered AFTER mount/unmount operation is successfully finished */
        .callback_premount_changed = nullptr, /*!< Pointer to the function callback that will be delivered BEFORE mount/unmount operation is started */
        .mount_config = {},
    };

    ESP_ERROR_CHECK(tinyusb_msc_storage_init_spiflash(&config_spi));
    ESP_ERROR_CHECK(tinyusb_msc_storage_mount(LV_FS_POSIX_PATH));

    static tusb_desc_device_t descriptor_config = {
        .bLength = sizeof(descriptor_config),
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

    static char const *string_desc_arr[] = {
        (const char[]){0x09, 0x04}, // 0: is supported language is English (0x0409)
        "TinyUSB",                  // 1: Manufacturer
        "TinyUSB Device",           // 2: Product
        "123456",                   // 3: Serials
        "Example MSC",              // 4. MSC
    };

    static uint8_t const desc_configuration[] = {
        // Config number, interface count, string index, total length, attribute, power in mA
        TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

        // Interface number, string index, EP Out & EP In address, EP size
        TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 0, EDPT_MSC_OUT, EDPT_MSC_IN, TUD_OPT_HIGH_SPEED ? 512 : 64),
    };

    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = &descriptor_config,
        .string_descriptor = string_desc_arr,
        .string_descriptor_count = sizeof(string_desc_arr) / sizeof(string_desc_arr[0]),
        .external_phy = false,
        .configuration_descriptor = desc_configuration,
        .self_powered = false,
        .vbus_monitor_io = 0,
    };

    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    ESP_ERROR_CHECK(tinyusb_msc_storage_unmount());
}

msc_application::~msc_application()
{
    ESP_ERROR_CHECK(tinyusb_msc_storage_mount(LV_FS_POSIX_PATH));

    tinyusb_msc_storage_deinit();

    ESP_ERROR_CHECK(wl_unmount(m_wl_handle));
}

void msc_application::on_create()
{
}

void msc_application::on_update(float timestep)
{
}
