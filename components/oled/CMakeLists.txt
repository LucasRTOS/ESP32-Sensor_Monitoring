idf_component_register(
    SRCS "oled.c"
    INCLUDE_DIRS "include"
    REQUIRES
        config
        esp_driver_i2c
        esp_lcd
        esp_lvgl_port
        lvgl
)