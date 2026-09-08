#pragma once

#include <stdint.h>
#include "esp_err.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"


typedef enum
{
    SENSOR_ID_LDR,
    SENSOR_ID_KY028,
    SENSOR_ID_HW504_X,
    SENSOR_ID_HW504_Y,
    SENSOR_ID_HW504_SW,

} sensor_id_t;


typedef enum
{
    SENSOR_TYPE_ANALOG,
    SENSOR_TYPE_DIGITAL,

} sensor_type_t;


typedef struct
{
    uint32_t sequence;
    uint32_t timestamp_ms;
    sensor_id_t sensor;
    sensor_type_t type;
    uint32_t raw_value;
    float value;

} sensor_sample_t;


esp_err_t sensor_init(QueueHandle_t queue);

