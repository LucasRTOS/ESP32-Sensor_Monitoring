#pragma once

#include "esp_err.h"
#include <stdbool.h>

esp_err_t oled_init(void);
void oled_update_display(float luminosity, float temperature, int joystick_x, int joystick_y, bool http_connected);