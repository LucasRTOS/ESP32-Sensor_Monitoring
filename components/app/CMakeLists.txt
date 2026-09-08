idf_component_register(
    SRCS "app.c"
    INCLUDE_DIRS "include"
    REQUIRES
        config
        oled
        sensor
        wifi_server
        nvs_flash
)