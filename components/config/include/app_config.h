#pragma once

//Configuration file

//Display
#define OLED_I2C_PORT              0
#define OLED_SDA_GPIO              21
#define OLED_SCL_GPIO              22
#define OLED_I2C_ADDRESS           0x3C
#define OLED_I2C_SPEED_HZ          (400 * 1000)
#define OLED_H_RES                 128
#define OLED_V_RES                 64

//Tasks
#define APP_TASK_STACK_SIZE        4096
#define APP_TASK_PRIORITY          5
#define APP_TASK_CORE              1

#define SENSOR_TASK_STACK_SIZE     4096
#define SENSOR_TASK_PRIORITY       5
#define SENSOR_TASK_CORE           1

//Queues
#define SENSOR_QUEUE_LENGTH        25
#define HTTP_QUEUE_LENGTH      1

//Sensors             "defining" GPIOs just to specify pins used
//#define LDR_ADC_GPIO             32
#define LDR_ADC_CHANNEL            4
#define LDR_SAMPLE_RATE_HZ         1
#define LDR_BRIGHT_VAL             4095
#define LDR_DARK_VAL               200

//#define KY028_ADC_GPIO           33
#define KY028_ADC_CHANNEL          5
#define KY028_SAMPLE_RATE_HZ       2

//#define HW504_X_ADC_GPIO         34
//#define HW504_Y_ADC_GPIO         35
#define HW504_X_ADC_CHANNEL         6
#define HW504_Y_ADC_CHANNEL         7
//#define HW504_SW_GPIO             27
#define HW504_SAMPLE_RATE_HZ        5

//KY-028 calibration
#define KY028_COLD_RAW              2000.0f
#define KY028_COLD_TEMP_C           0.0f
#define KY028_ROOM_RAW              1390.0f
#define KY028_ROOM_TEMP_C           21.0f
#define KY028_HOT_RAW               920.0f
#define KY028_HOT_TEMP_C            100.0f

//HW-504 (Joystick) calibration
#define HW504_X_CENTER              2020
#define HW504_Y_CENTER              2035
#define HW504_DEAD_ZONE             75

//Wi-Fi Confi
#define WIFI_SSID                   "SSID0"
#define WIFI_PASSWORD               "PASSWORD"
#define WIFI_CONNECT_TIMEOUT_MS     10000
#define WIFI_RECONNECT_DELAY_MS     5000
#define WIFI_RECONNECT_TASK_STACK_SIZE 4096
#define WIFI_RECONNECT_TASK_PRIORITY   5
#define WIFI_RECONNECT_TASK_CORE       0
#define HTTP_PORT 80

