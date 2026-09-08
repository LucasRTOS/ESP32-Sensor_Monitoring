#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdint.h>

typedef struct {
    uint32_t ldr_raw;
    float ldr_value;
    uint32_t ky028_raw;
    float ky028_value;
    uint32_t joystick_x_raw;
    int joystick_x;
    uint32_t joystick_y_raw;
    int joystick_y;
    uint32_t joystick_sw_raw;
    int joystick_sw;
} http_data_t;

esp_err_t wifi_server_init(QueueHandle_t http_queue);
bool wifi_server_is_connected(void);