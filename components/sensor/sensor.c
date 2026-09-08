#include "sensor.h"
#include "app_config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_adc/adc_oneshot.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"

static const char *TAG = "SENSOR";


static QueueHandle_t s_sensor_queue = NULL;
static adc_oneshot_unit_handle_t s_adc1_handle = NULL;
static uint32_t s_sequence = 0;
static portMUX_TYPE s_sequence_lock = portMUX_INITIALIZER_UNLOCKED;

static uint32_t next_sequence(void);

static esp_err_t ldr_init(void);
static void ldr_task(void *arg);
static esp_err_t ky028_init(void);
static void ky028_task(void *arg);
static esp_err_t hw504_init(void);
static void hw504_task(void *arg);

static float ldr_lum_conversion_percentage(int raw);
static float ky028_temp_conversion(int raw);
static int hw504_apply_dead_zone(int raw, int center);


esp_err_t sensor_init(QueueHandle_t queue)
{
    if (queue == NULL)
    {
        ESP_LOGE(TAG, "Invalid sensor queue");

        return ESP_ERR_INVALID_ARG;
    }

    s_sensor_queue = queue;

    
    ESP_LOGI(TAG, "Initializing sensors...");

    esp_err_t err = ldr_init();
    if (err != ESP_OK)
    {
        return err;
    }

    err = ky028_init();
    if (err != ESP_OK)
    {
        return err;
    }

    err = hw504_init();
    if (err != ESP_OK)
    {
        return err;
    }

    BaseType_t ret = xTaskCreatePinnedToCore(ldr_task, "ldr_task", SENSOR_TASK_STACK_SIZE, NULL, SENSOR_TASK_PRIORITY, NULL, SENSOR_TASK_CORE);
    if (ret != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create LDR task");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "LDR task created");

    ret = xTaskCreatePinnedToCore(ky028_task,"ky028_task",SENSOR_TASK_STACK_SIZE,NULL,SENSOR_TASK_PRIORITY,NULL,SENSOR_TASK_CORE);
    if (ret != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create KY-028 task");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "KY-028 task created");

    ret = xTaskCreatePinnedToCore(hw504_task, "hw504_task", SENSOR_TASK_STACK_SIZE, NULL, SENSOR_TASK_PRIORITY, NULL, SENSOR_TASK_CORE);
    if (ret != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create HW-504 task");
        return ESP_FAIL;
    }

    return ESP_OK;
}


static esp_err_t ldr_init(void)
{
    adc_oneshot_unit_init_cfg_t adc_config = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };

    esp_err_t err = adc_oneshot_new_unit(&adc_config, &s_adc1_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize ADC1");
        return err;
    }


    adc_oneshot_chan_cfg_t channel_config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    err = adc_oneshot_config_channel(s_adc1_handle, (adc_channel_t)LDR_ADC_CHANNEL, &channel_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure LDR ADC channel");
        return err;
    }

    ESP_LOGI(TAG, "LDR initialized");

    return ESP_OK;
}

static esp_err_t ky028_init(void)
{
    adc_oneshot_chan_cfg_t channel_config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    esp_err_t err = adc_oneshot_config_channel(s_adc1_handle, (adc_channel_t)KY028_ADC_CHANNEL, &channel_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure KY-028 ADC channel");
        return err;
    }

    ESP_LOGI(TAG, "KY-028 initialized");

    return ESP_OK;
}

static esp_err_t hw504_init(void)
{
    adc_oneshot_chan_cfg_t channel_config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    esp_err_t err = adc_oneshot_config_channel(s_adc1_handle, (adc_channel_t)HW504_X_ADC_CHANNEL, &channel_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure HW-504 X ADC channel");
        return err;
    }

    err = adc_oneshot_config_channel(s_adc1_handle, (adc_channel_t)HW504_Y_ADC_CHANNEL, &channel_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure HW-504 Y ADC channel");
        return err;
    }

    ESP_LOGI(TAG, "HW-504 initialized");

    return ESP_OK;
}

static void ldr_task(void *arg)
{
    sensor_sample_t sample;

    while (1)
    {
        int raw_value = 0;

        esp_err_t err = adc_oneshot_read(s_adc1_handle, (adc_channel_t)LDR_ADC_CHANNEL, &raw_value);
        if (err != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to read LDR");
        }
        else
        {
            sample.sequence = next_sequence();
            sample.timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000);
            sample.sensor = SENSOR_ID_LDR;
            sample.type = SENSOR_TYPE_ANALOG;

            sample.raw_value = raw_value;
            sample.value = (float)ldr_lum_conversion_percentage(raw_value);

            
            if (xQueueSend(s_sensor_queue, &sample, pdMS_TO_TICKS(10)) != pdPASS)
            {
                ESP_LOGW(TAG, "Sensor queue full");
            }
            
        }

        vTaskDelay(pdMS_TO_TICKS(1000 / LDR_SAMPLE_RATE_HZ));
    }
}

static float ldr_lum_conversion_percentage(int raw)
{
    float lum_percentage;

    lum_percentage = ((float)raw - LDR_DARK_VAL)* 100.0f/(LDR_BRIGHT_VAL - LDR_DARK_VAL);

    if (lum_percentage < 0.0f)
    {
        lum_percentage = 0.0f;
    }
    else if (lum_percentage > 100.0f)
    {
        lum_percentage = 100.0f;
    }
    
    return lum_percentage;
}

static void ky028_task(void *arg)
{
    sensor_sample_t sample;

    while (1)
    {
        int raw_value = 0;

        esp_err_t err = adc_oneshot_read(s_adc1_handle, (adc_channel_t)KY028_ADC_CHANNEL, &raw_value);
        if (err != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to read KY-028");
        }
        else
        {
            sample.sequence = next_sequence();
            sample.timestamp_ms =(uint32_t)(esp_timer_get_time() / 1000);
            sample.sensor = SENSOR_ID_KY028;
            sample.type = SENSOR_TYPE_ANALOG;

            sample.raw_value = raw_value;
            sample.value = ky028_temp_conversion(raw_value);

            if (xQueueSend(s_sensor_queue, &sample, pdMS_TO_TICKS(10)) != pdPASS)
            {
                ESP_LOGW(TAG, "Sensor queue full");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000 / KY028_SAMPLE_RATE_HZ));
    }
}

static float ky028_temp_conversion(int raw)
{
    float temperature;

    if (raw >= KY028_ROOM_RAW)
    {
        temperature = KY028_COLD_TEMP_C + ((float)raw - KY028_COLD_RAW) * (KY028_ROOM_TEMP_C - KY028_COLD_TEMP_C) / (KY028_ROOM_RAW - KY028_COLD_RAW);
    }
    else
    {
        temperature = KY028_ROOM_TEMP_C + (KY028_ROOM_RAW - (float)raw) * (KY028_HOT_TEMP_C - KY028_ROOM_TEMP_C) / (KY028_ROOM_RAW - KY028_HOT_RAW);
    }

    
    return temperature;
}

static void hw504_task(void *arg)
{
    sensor_sample_t sample;

    while (1)
    {
        int raw_x = 0;
        int raw_y = 0;

        esp_err_t err_x = adc_oneshot_read(s_adc1_handle, (adc_channel_t)HW504_X_ADC_CHANNEL, &raw_x);
        esp_err_t err_y = adc_oneshot_read(s_adc1_handle, (adc_channel_t)HW504_Y_ADC_CHANNEL, &raw_y);
        if (err_x == ESP_OK)
        if (err_x == ESP_OK)
        {
            sample.sequence = next_sequence();
            sample.timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000);
            sample.sensor = SENSOR_ID_HW504_X;
            sample.type = SENSOR_TYPE_ANALOG;

            sample.raw_value = raw_x;
            sample.value = (float)hw504_apply_dead_zone(raw_x, HW504_X_CENTER);

            if (xQueueSend(s_sensor_queue, &sample, pdMS_TO_TICKS(10)) != pdPASS)
            {
                ESP_LOGW(TAG, "Sensor queue full");
            }
        }
        if (err_y == ESP_OK)
        {
            sample.sequence = next_sequence();
            sample.timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000);
            sample.sensor = SENSOR_ID_HW504_Y;
            sample.type = SENSOR_TYPE_ANALOG;
            sample.raw_value = raw_y;
            raw_y = hw504_apply_dead_zone(raw_y, HW504_Y_CENTER);
            sample.value = (float)raw_y;

            if (xQueueSend(s_sensor_queue, &sample, pdMS_TO_TICKS(10)) != pdPASS)
            {
                ESP_LOGW(TAG, "Sensor queue full");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000 / HW504_SAMPLE_RATE_HZ));
    }
}

static int hw504_apply_dead_zone(int raw, int center)
{
    if (raw >= center - HW504_DEAD_ZONE && raw <= center + HW504_DEAD_ZONE)
    {
        return center;
    }

    return raw;
}

static uint32_t next_sequence(void)
{
    uint32_t sequence;

    portENTER_CRITICAL(&s_sequence_lock);
    sequence = s_sequence++;
    portEXIT_CRITICAL(&s_sequence_lock);

    return sequence;
}
