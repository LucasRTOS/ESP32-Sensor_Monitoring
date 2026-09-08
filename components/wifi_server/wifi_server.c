#include "wifi_server.h"
#include "app_config.h"
#include <stdio.h>
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_http_server.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static const char *TAG = "WIFI_SERVER";

static httpd_handle_t s_http_server = NULL;
static QueueHandle_t s_http_queue = NULL;
static bool s_http_connected = false;
static TaskHandle_t s_wifi_reconnect_task = NULL;

static esp_err_t sensor_handler(httpd_req_t *req)
{
    http_data_t data;

    if (s_http_queue == NULL)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "HTTP queue unavailable");
        return ESP_FAIL;
    }

    if (xQueuePeek(s_http_queue, &data, 0) != pdPASS)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Sensor data unavailable");
        return ESP_FAIL;
    }

    char json[320];

    int len = snprintf(
        json,
        sizeof(json),
        "{\"ldr\":{\"raw\":%lu,\"value\":%.1f},"
        "\"ky028\":{\"raw\":%lu,\"value\":%.1f},"
        "\"joystick\":{\"x\":{\"raw\":%lu,\"value\":%d},"
        "\"y\":{\"raw\":%lu,\"value\":%d},"
        "\"sw\":{\"raw\":%lu,\"value\":%d}}}",
        (unsigned long)data.ldr_raw,
        data.ldr_value,
        (unsigned long)data.ky028_raw,
        data.ky028_value,
        (unsigned long)data.joystick_x_raw,
        data.joystick_x,
        (unsigned long)data.joystick_y_raw,
        data.joystick_y,
        (unsigned long)data.joystick_sw_raw,
        data.joystick_sw
    );

    if (len < 0 || len >= (int)sizeof(json))
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to build JSON");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, len);

    ESP_LOGI(TAG, "HTTP GET /api/sensors -> %s", json);

    return ESP_OK;
}

static esp_err_t root_handler(httpd_req_t *req)
{
    const char *response =
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<meta charset=\"UTF-8\">"
        "<title>ESP32 Sensor Monitor</title>"
        "</head>"
        "<body>"
        "<h1>ESP32 Sensor Monitor</h1>"
        "<p>HTTP server is running.</p>"
        "<p>Sensor endpoint: <a href=\"/api/sensors\">/api/sensors</a></p>"
        "</body>"
        "</html>";

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

static esp_err_t start_http_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    config.server_port = HTTP_PORT;

    esp_err_t err = httpd_start(&s_http_server, &config);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(err));
        return err;
    }

    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_handler,
        .user_ctx = NULL,
    };

    err = httpd_register_uri_handler(s_http_server, &root_uri);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register root URI: %s", esp_err_to_name(err));
        return err;
    }

    httpd_uri_t sensor_uri = {
        .uri = "/api/sensors",
        .method = HTTP_GET,
        .handler = sensor_handler,
        .user_ctx = NULL,
    };

    err = httpd_register_uri_handler(s_http_server, &sensor_uri);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register sensor URI: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "HTTP server started on port %d", HTTP_PORT);
    ESP_LOGI(TAG, "Sensor endpoint: /api/sensors");

    return ESP_OK;
}

static void wifi_reconnect_task(void *arg)
{
    while (1)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        vTaskDelay(pdMS_TO_TICKS(WIFI_RECONNECT_DELAY_MS));

        ESP_LOGI(TAG, "Attempting Wi-Fi reconnection...");

        esp_err_t err = esp_wifi_connect();

        if (err != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to reconnect Wi-Fi: %s", esp_err_to_name(err));
        }
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "Wi-Fi station started");
                esp_wifi_connect();
                break;

            case WIFI_EVENT_STA_DISCONNECTED:
            {
                wifi_event_sta_disconnected_t *event =
                    (wifi_event_sta_disconnected_t *)event_data;

                s_http_connected = false;

                ESP_LOGW(TAG, "Wi-Fi disconnected, reason=%d", event->reason);

                if (s_wifi_reconnect_task != NULL)
                {
                    xTaskNotifyGive(s_wifi_reconnect_task);
                }

                break;
            }

            default:
                break;
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;

        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));

        if (s_http_server == NULL)
        {
            esp_err_t err = start_http_server();

            if (err != ESP_OK)
            {
                s_http_connected = false;

                ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(err));
            }
            else
            {
                s_http_connected = true;

                ESP_LOGI(TAG, "HTTP connection status: ONLINE");
            }
        }
        else
        {
            s_http_connected = true;

            ESP_LOGI(TAG, "HTTP connection status: ONLINE");
        }
    }
}

static esp_err_t wifi_init(void)
{
    esp_err_t err = esp_netif_init();

    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
    {
        ESP_LOGE(TAG, "Failed to initialize network interface: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_event_loop_create_default();

    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
    {
        ESP_LOGE(TAG, "Failed to create event loop: %s", esp_err_to_name(err));
        return err;
    }

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    err = esp_wifi_init(&cfg);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize Wi-Fi: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register Wi-Fi event handler: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register IP event handler: %s", esp_err_to_name(err));
        return err;
    }

    BaseType_t task_result = xTaskCreatePinnedToCore(wifi_reconnect_task, "wifi_reconnect_task", WIFI_RECONNECT_TASK_STACK_SIZE, NULL, WIFI_RECONNECT_TASK_PRIORITY, &s_wifi_reconnect_task, WIFI_RECONNECT_TASK_CORE);

        if (task_result != pdPASS)
        {
            ESP_LOGE(TAG, "Failed to create Wi-Fi reconnect task");
            return ESP_FAIL;
        }

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };

    err = esp_wifi_set_mode(WIFI_MODE_STA);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set Wi-Fi mode: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set Wi-Fi configuration: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_wifi_start();

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start Wi-Fi: %s", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

esp_err_t wifi_server_init(QueueHandle_t http_queue)
{
    if (http_queue == NULL)
    {
        ESP_LOGE(TAG, "HTTP queue is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    s_http_queue = http_queue;

    return wifi_init();
}

bool wifi_server_is_connected(void)
{
    return s_http_connected;
}