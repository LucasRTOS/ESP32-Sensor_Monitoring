idf_component_register(
    SRCS "wifi_server.c"
    INCLUDE_DIRS "include"
    REQUIRES
        config
        esp_wifi
        esp_event
        esp_netif
        esp_http_server
        nvs_flash
)