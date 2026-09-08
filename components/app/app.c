#include "app.h"
#include "app_config.h"
#include "oled.h"
#include "sensor.h"
#include "wifi_server.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "APP";

static QueueHandle_t s_sensor_queue = NULL;
static QueueHandle_t s_http_queue = NULL;


static void app_task(void *arg)
{
    sensor_sample_t sample;

    http_data_t data = {
        .ldr_raw = 0,
        .ldr_value = 0.0f,
        .ky028_raw = 0,
        .ky028_value = 0.0f,
        .joystick_x_raw = HW504_X_CENTER,
        .joystick_x = HW504_X_CENTER,
        .joystick_y_raw = HW504_Y_CENTER,
        .joystick_y = HW504_Y_CENTER,
        .joystick_sw_raw = 0,
        .joystick_sw = 0,
    };

    while (1)
    {
        if (xQueueReceive(s_sensor_queue, &sample, portMAX_DELAY) != pdPASS)
        {
            continue;
        }

        switch (sample.sensor)
        {
            case SENSOR_ID_LDR:
                data.ldr_raw = sample.raw_value;
                data.ldr_value = sample.value;
                break;

            case SENSOR_ID_KY028:
                data.ky028_raw = sample.raw_value;
                data.ky028_value = sample.value;
                break;

            case SENSOR_ID_HW504_X:
                data.joystick_x_raw = sample.raw_value;
                data.joystick_x = (int)sample.value;
                break;

            case SENSOR_ID_HW504_Y:
                data.joystick_y_raw = sample.raw_value;
                data.joystick_y = (int)sample.value;
                break;

            case SENSOR_ID_HW504_SW:
                data.joystick_sw_raw = sample.raw_value;
                data.joystick_sw = (int)sample.value;
                break;

            default:
                break;
        }
        
        oled_update_display(
            data.ldr_value,
            data.ky028_value,
            data.joystick_x,
            data.joystick_y,
            wifi_server_is_connected()
        );

        xQueueOverwrite(s_http_queue, &data);
    }
}

esp_err_t app_init(void)
{
    esp_err_t err = nvs_flash_init();

    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        err = nvs_flash_erase();

        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to erase NVS: %s", esp_err_to_name(err));
            return err;
        }

        err = nvs_flash_init();
    }

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(err));
        return err;
    }

    err = oled_init();

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize OLED: %s", esp_err_to_name(err));
        return err;
    }

    s_sensor_queue = xQueueCreate(
        SENSOR_QUEUE_LENGTH,
        sizeof(sensor_sample_t)
    );

    if (s_sensor_queue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create sensor queue");
        return ESP_ERR_NO_MEM;
    }

    s_http_queue = xQueueCreate(HTTP_QUEUE_LENGTH, sizeof(http_data_t));

    if (s_http_queue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create HTTP queue");
        return ESP_ERR_NO_MEM;
    }

    http_data_t initial_data = {
        .ldr_raw = 0,
        .ldr_value = 0.0f,
        .ky028_raw = 0,
        .ky028_value = 0.0f,
        .joystick_x_raw = HW504_X_CENTER,
        .joystick_x = HW504_X_CENTER,
        .joystick_y_raw = HW504_Y_CENTER,
        .joystick_y = HW504_Y_CENTER,
        .joystick_sw_raw = 0,
        .joystick_sw = 0,
    };

    xQueueOverwrite(s_http_queue, &initial_data);

    err = sensor_init(s_sensor_queue);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize sensors: %s", esp_err_to_name(err));
        return err;
    }

    err = wifi_server_init(s_http_queue);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize HTTP server: %s", esp_err_to_name(err));
        return err;
    }

    BaseType_t task_result = xTaskCreatePinnedToCore(app_task, "app_task", APP_TASK_STACK_SIZE, NULL, APP_TASK_PRIORITY, NULL, APP_TASK_CORE);

    if (task_result != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create app task");
        return ESP_FAIL;
    }

    return ESP_OK;
}