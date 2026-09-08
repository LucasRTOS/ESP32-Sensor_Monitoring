#include "oled.h"
#include "app_config.h"
#include <stdio.h>
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_ssd1306.h"

#include "esp_lvgl_port.h"
#include "lvgl.h"

static const char *TAG = "OLED";


static i2c_master_bus_handle_t s_i2c_bus = NULL;
static esp_lcd_panel_io_handle_t s_io_handle = NULL;
static esp_lcd_panel_handle_t s_panel_handle = NULL;
static lv_display_t *s_display = NULL;


esp_err_t oled_init(void)
{
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Initializing OLED...");

    i2c_master_bus_config_t bus_config = {
        .i2c_port = OLED_I2C_PORT,
        .sda_io_num = OLED_SDA_GPIO,
        .scl_io_num = OLED_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ret = i2c_new_master_bus(&bus_config, &s_i2c_bus);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2C bus");
        return ret;
    }

    //LCD config struct
    esp_lcd_panel_io_i2c_config_t io_config = {
        .dev_addr = OLED_I2C_ADDRESS,
        .scl_speed_hz = OLED_I2C_SPEED_HZ,
        .control_phase_bytes = 1,
        .dc_bit_offset = 6,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };

    ESP_LOGI(TAG, "Initializing LCD I2C...");
    ret = esp_lcd_new_panel_io_i2c(s_i2c_bus, &io_config, &s_io_handle);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create LCD I2C interface");
        return ret;
    }

    //LCD painel config
    esp_lcd_panel_dev_config_t panel_config = {
        .bits_per_pixel = 1,
        .reset_gpio_num = -1,
    };

    ret = esp_lcd_new_panel_ssd1306(s_io_handle, &panel_config, &s_panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create SSD1306 panel");
        return ret;
    }


    ESP_LOGI(TAG, "Initializing OLED panel");
    ret = esp_lcd_panel_reset(s_panel_handle);

    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_lcd_panel_init(s_panel_handle);

    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_lcd_panel_disp_on_off(s_panel_handle, true);

    if (ret != ESP_OK) {
        return ret;
    }


    //LVG config
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();

    ESP_LOGI(TAG, "Initializing LVGL");
    ret = lvgl_port_init(&lvgl_cfg);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize LVGL");
        return ret;
    }


    //LVG-display comm
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = s_io_handle,
        .panel_handle = s_panel_handle,

        .buffer_size = OLED_H_RES * OLED_V_RES,

        .double_buffer = true,

        .hres = OLED_H_RES,
        .vres = OLED_V_RES,

        .monochrome = true,

        .rotation = {
            .swap_xy = false,
            .mirror_x = true,
            .mirror_y = true,
        },

        .flags = {
            .sw_rotate = false,
        },
    };
    
    ESP_LOGI(TAG, "Creating Display");
    s_display = lvgl_port_add_disp(&display_cfg);

    if (s_display == NULL) {
        ESP_LOGE(TAG, "Failed to add LVGL display");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "OLED initialized");

    return ESP_OK;
}


void oled_update_display(float luminosity, float temperature, int joystick_x, int joystick_y, bool http_connected)
{
    static lv_obj_t *label = NULL;

    if (lvgl_port_lock(1000) != pdTRUE)
    {
        return;
    }

    if (label == NULL)
    {
        label = lv_label_create(lv_screen_active());
        lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, 0);
    }

    char text[128];

    snprintf(text, sizeof(text), "LDR: %.1f %%\n" "TEMP: %.1f C\n" "X: %04d  Y: %04d\n" "HTTP: %s",
        luminosity, temperature, joystick_x, joystick_y, http_connected ? "ONLINE" : "OFFLINE");

    lv_label_set_text(label, text);

    lvgl_port_unlock();
}