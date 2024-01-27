#include "hardware/storage.h"

#include <map>
#include <string>

#include <esp_littlefs.h>

namespace hardware
{
    namespace storage
    {
        static const char partition_label[] = "storage";

        void mount(const type storage_type, const char *mount_point)
        {
            if (storage_type != type::internal || esp_littlefs_mounted(partition_label))
                return;

            const esp_vfs_littlefs_conf_t littlefs_config = {
                .base_path = mount_point,
                .partition_label = partition_label,
                .partition = nullptr,
                .format_if_mount_failed = true,
                .read_only = false,
                .dont_mount = false,
                .grow_on_mount = true,
            };

            ESP_ERROR_CHECK(esp_vfs_littlefs_register(&littlefs_config));
        }

        void unmount(const type storage_type)
        {
            if (storage_type != type::internal || !esp_littlefs_mounted(partition_label))
                return;

            ESP_ERROR_CHECK(esp_vfs_littlefs_unregister(partition_label));
        }
    }
}