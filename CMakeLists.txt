file(GLOB_RECURSE SOURCES "src/*.c" "src/*.cpp")

idf_component_register(SRCS ${SOURCES} INCLUDE_DIRS "include" PRIV_INCLUDE_DIRS "src" REQUIRES lvgl PRIV_REQUIRES driver nvs_flash fatfs esp_lcd esp_adc esp_wifi)
