# ESP32 Sensor Monitoring System

Firmware developed for an ESP32-based sensor monitoring system using ESP-IDF.

The system reads data from multiple sensors, displays the information on an OLED screen, connects to a Wi-Fi network, and provides the sensor data through an HTTP server.

The architecture is designed to keep hardware access, application logic, display handling, and network communication decoupled.

## Features

- ESP32 firmware developed with ESP-IDF
- FreeRTOS-based architecture
- Modular component-based structure
- OLED display using LVGL
- LDR luminosity measurement
- KY-028 temperature measurement
- HW-504 joystick input
- Wi-Fi connectivity
- HTTP communication
- JSON-formatted sensor data
- Centralized configuration
- Sensor data communication using FreeRTOS queues
- Multicore task allocation

---

## Project Structure

```text
FIRMWARE_APP/
├── main/
│   ├── CMakeLists.txt
│   └── main.c
│
├── components/
│   ├── config/
│   │   ├── CMakeLists.txt
│   │   └── include/
│   │       └── app_config.h
│   │
│   ├── oled/
│   │   ├── CMakeLists.txt
│   │   ├── oled.c
│   │   └── include/
│   │       └── oled.h
│   │
│   ├── sensor/
│   │   ├── CMakeLists.txt
│   │   ├── sensor.c
│   │   └── include/
│   │       └── sensor.h
│   │
│   ├── wifi_server/
│   │   ├── CMakeLists.txt
│   │   ├── wifi_server.c
│   │   └── include/
│   │       └── wifi_server.h
│   │
│   └── app/
│       ├── CMakeLists.txt
│       ├── app.c
│       └── include/
│           └── app.h
│
├── managed_components/
│
└── CMakeLists.txt
```

---

## System Architecture

The firmware is divided into independent components.

```text
                    ┌───────────────┐
                    │     main.c    │
                    └───────┬───────┘
                            │
                            ▼
                    ┌───────────────┐
                    │     app.c     │
                    └───────┬───────┘
                            │
          ┌─────────────────┼─────────────────┐
          │                 │                 │
          ▼                 ▼                 ▼
    ┌───────────┐     ┌───────────┐     ┌──────────────┐
    │  Sensor   │     │   OLED    │     │ Wi-Fi / HTTP │
    │ Component │     │ Component │     │   Component  │
    └─────┬─────┘     └───────────┘     └──────┬───────┘
          │                                     │
          │                                     │
          ▼                                     │
    ┌───────────────┐                           │
    │ Sensor Queue  │───────────────────────────┘
    └───────────────┘
```

The `app` component is responsible for coordinating the different modules.

The sensor component is responsible for acquiring sensor data and sending the measurements through a FreeRTOS queue.

The OLED component is responsible for displaying the current sensor values.

The Wi-Fi/HTTP component is responsible for network connectivity and exposing the sensor data through an HTTP server.

---

## Communication Flow

The communication between the ESP32 and a client is performed using HTTP requests.

```text
Browser
   │
   │ HTTP GET request
   ▼
ESP32 HTTP Server
   │
   │ JSON sensor data
   ▼
Browser Dashboard
```

Each HTTP request returns the current sensor state as a JSON response.

The client can periodically poll the ESP32 with HTTP requests to obtain updated sensor values.

The main sensor endpoint is:

```text
/api/sensors
```

---

## Sensors

### LDR

The LDR is connected to an ADC input and is used to measure luminosity.

The firmware converts the raw ADC value into a percentage between 0% and 100%.

Example:

```text
LDR: 75.4 %
```

The luminosity conversion is based on the configured dark and bright reference values.

---

### KY-028

The KY-028 module is used for temperature measurement.

The firmware uses calibration points to convert the raw ADC value into temperature in degrees Celsius.

The current calibration uses three reference points:

```text
Cold:
Raw = 2000
Temperature = 0 °C

Room:
Raw = 1390
Temperature = 21 °C

Hot:
Raw = 920
Temperature = 100 °C
```

A piecewise linear conversion is used between the calibration points.

Example:

```text
TEMP: 21.0 C
```

---

### HW-504 Joystick

The HW-504 joystick provides:

- X-axis
- Y-axis
- Push-button switch (not-implemented in this version)

The X and Y axes are read through ADC inputs.

A dead zone is applied around the calibrated center position to prevent small fluctuations when the joystick is at rest.

Example:

```text
X: 2020
Y: 2035
```

The joystick switch can be read as a digital input.

---

## Sampling Rates

Each sensor module runs at its own sampling rate.

```text
LDR       → 1 Hz
KY-028    → 2 Hz
HW-504    → 5 Hz
```

The different sampling rates allow each sensor to be updated according to its expected behavior without unnecessarily increasing CPU usage.

---

## FreeRTOS Architecture

The firmware uses FreeRTOS tasks and queues.

The sensor component creates independent tasks for the logical sensor modules.

```text
LDR Task
   │
   ├──────────────┐
   │              │
KY-028 Task       │
   │              │
   ├──────────────┤
   │              │
HW-504 Task       │
   │              │
   ▼              ▼
      Sensor Queue
           │
           ▼
       App Task
       /      \
      ▼        ▼
    OLED      HTTP
```

Sensor tasks are responsible only for reading and processing sensor data.

The application task receives the sensor samples and updates the rest of the system.

The HTTP layer receives a snapshot of the current sensor state and provides it to clients through the HTTP endpoint.

The OLED component is updated by the application layer rather than directly by the sensor tasks.

This keeps the components decoupled and prevents the sensor tasks from directly manipulating the display.

---

## Multicore Usage

The ESP32 has two CPU cores.

The firmware uses the following task allocation:

```text
Core 0
└── Wi-Fi / HTTP application-related processing

Core 1
├── Application task
└── Sensor tasks
```

The application and sensor tasks are pinned to Core 1.

The Wi-Fi/HTTP application-related task is assigned to Core 0 when applicable.

ESP-IDF internal Wi-Fi tasks are managed by the framework and are not manually pinned by the application.

---

## OLED Display

The system uses an SSD1306 OLED display controlled through I2C.

The display resolution is:

```text
128 x 64 pixels
```

The display uses LVGL through `esp_lvgl_port`.

The OLED shows the current sensor values and the HTTP server status.

Example:

```text
LDR: 75.4 %
TEMP: 21.0 C
X: 2020  Y: 2035
HTTP: ONLINE
```

The HTTP status indicates whether the HTTP server is currently available.

---

## Wi-Fi

The ESP32 operates as a Wi-Fi station and connects to the configured access point.

The SSID and password are defined in the centralized configuration file:

```text
components/config/include/app_config.h
```

Example:

```c
#define WIFI_SSID                   "SSID"
#define WIFI_PASSWORD               "PASSWORD"
```

The firmware also defines configurable connection timeout and reconnection delay values.

```c
#define WIFI_CONNECT_TIMEOUT_MS     10000
#define WIFI_RECONNECT_DELAY_MS     5000
```

If the Wi-Fi connection is lost, the firmware attempts to reconnect automatically.

The HTTP server is stopped when the Wi-Fi connection is lost and restarted when the ESP32 receives a valid IP address again.

---

## HTTP Server

The firmware includes an embedded HTTP server running on port 80.

```text
HTTP Port: 80
```

The server provides the following endpoints.

### Root Endpoint

```text
/
```

The root endpoint provides a simple HTML page containing basic information about the server and a link to the sensor endpoint.

---

### Sensor Endpoint

```text
/api/sensors
```

This endpoint returns the current sensor data in JSON format.

Example request:

```text
http://<ESP32_IP>/api/sensors
```

Example response:

```json
{
    "ldr": {
        "raw": 3500,
        "value": 89.7
    },
    "ky028": {
        "raw": 1390,
        "value": 21.0
    },
    "joystick": {
        "x": {
            "raw": 2020,
            "value": 2020
        },
        "y": {
            "raw": 2035,
            "value": 2035
        },
        "sw": {
            "raw": 1,
            "value": 1
        }
    }
}
```

The endpoint can be accessed directly from a browser, command-line tool, or another application capable of making HTTP requests.

---

## HTTP Data

The sensor data is made available through the HTTP server as JSON.

The browser or another client retrieves the current sensor state by sending an HTTP GET request to:

```text
/api/sensors
```

For example:

```text
http://<ESP32_IP>/api/sensors
```

The ESP32 responds with a JSON object containing:

- LDR raw value
- LDR luminosity percentage
- KY-028 raw value
- KY-028 temperature
- HW-504 X raw value
- HW-504 X processed value
- HW-504 Y raw value
- HW-504 Y processed value
- HW-504 switch raw value
- HW-504 switch processed value

The client can repeat the HTTP request periodically if continuous monitoring is required.

---

## JSON Communication

The HTTP server uses JSON to organize the sensor data.

The JSON structure is divided into sensor groups:

```text
ldr
ky028
joystick
```

Each sensor contains its raw ADC or digital value and its processed value when applicable.

This structure makes the data easy to consume from a web interface, script, application, or other HTTP client.

---

## Queue Communication

Sensor measurements are transmitted between components using a FreeRTOS queue.

The sensor component creates `sensor_sample_t` structures containing information such as:

```text
Sequence number
Timestamp
Sensor identifier
Sensor type
Raw value
Processed value
```

The application task receives these structures and uses them to update the current system state.

The HTTP layer accesses the latest sensor state and returns a snapshot through the `/api/sensors` endpoint.

The queue provides decoupling between sensor acquisition and application processing.

---

## Sequence Numbers

Each sensor sample receives a unique sequence number.

The sequence number is generated centrally inside the sensor component.

A critical section protects access to the shared sequence counter because multiple sensor tasks can generate samples concurrently.

This ensures that samples receive consistent sequence numbers even when multiple sensor tasks are running at the same time.

---

## Centralized Configuration

Hardware parameters, task settings, queue sizes, sensor calibration values, and Wi-Fi configuration are centralized in:

```text
components/config/include/app_config.h
```

This includes:

- GPIO assignments
- ADC channels
- ADC sampling rates
- OLED configuration
- Task stack sizes
- Task priorities
- Core assignments
- Queue lengths
- Sensor calibration values
- Joystick center positions
- Joystick dead zone
- Wi-Fi SSID
- Wi-Fi password
- Wi-Fi timeout values
- HTTP port

Centralizing these values makes the firmware easier to configure and maintain.

---

## Error Handling

The firmware uses ESP-IDF error codes and logging mechanisms.

Initialization functions return `esp_err_t` values.

Errors are logged using ESP-IDF logging macros such as:

```c
ESP_LOGE()
ESP_LOGW()
ESP_LOGI()
```

Examples of situations handled by the firmware include:

- Invalid queue handles
- Failed peripheral initialization
- Failed Wi-Fi initialization
- Failed HTTP server initialization
- Failed HTTP URI registration
- Sensor data unavailable
- Wi-Fi disconnection
- Failure to build the JSON response

---

## Web Interface

The firmware includes a basic HTTP interface.

The main sensor data endpoint is:

```text
/api/sensors
```

After the ESP32 connects to Wi-Fi and receives an IP address, the endpoint can be accessed from another device connected to the same network.

For example:

```text
http://<ESP32_IP>/api/sensors
```

The returned JSON can be used by a web interface or any other HTTP client.

A browser can also access the root page:

```text
http://<ESP32_IP>/
```

The root page provides a link to the sensor endpoint.

---

## Example Testing

After connecting the ESP32 to the network, the IP address can be identified through the serial monitor.

The sensor endpoint can then be tested using a browser or PowerShell.

### Browser

```text
http://192.168.0.72/api/sensors
```

### PowerShell

```powershell
curl http://192.168.0.72/api/sensors
```

Or:

```powershell
(Invoke-WebRequest http://192.168.0.72/api/sensors).Content | ConvertFrom-Json | ConvertTo-Json -Depth 5
```

For continuous monitoring:

```powershell
while ($true) {
    (Invoke-WebRequest http://192.168.0.72/api/sensors).Content
    Start-Sleep -Milliseconds 1000
}
```

The polling interval can be adjusted according to the desired monitoring frequency.

---

## Dependencies

The project uses ESP-IDF components and managed components.

Main dependencies include:

- ESP-IDF
- FreeRTOS
- LVGL
- `esp_lvgl_port`
- ESP-IDF HTTP server
- ESP-IDF Wi-Fi driver
- ESP-IDF I2C driver
- ESP-IDF LCD/SSD1306 support

---

## Technologies

- ESP32
- ESP-IDF
- C
- FreeRTOS
- LVGL
- I2C
- ADC
- Wi-Fi
- HTTP
- JSON
- SSD1306 OLED

---

## Configuration

Before compiling the firmware, configure the following values in:

```text
components/config/include/app_config.h
```

### Wi-Fi

```c
#define WIFI_SSID                   "SSID"
#define WIFI_PASSWORD               "PASSWORD"
```

### OLED

```c
#define OLED_I2C_PORT               0
#define OLED_SDA_GPIO               21
#define OLED_SCL_GPIO               22
#define OLED_I2C_ADDRESS            0x3C
#define OLED_I2C_SPEED_HZ           (400 * 1000)
#define OLED_H_RES                  128
#define OLED_V_RES                  64
```

### Sensors

Configure the GPIOs, ADC channels, sampling rates, and calibration values according to the connected hardware.

---

## Build

The project can be built using ESP-IDF tools.

From the project directory:

```powershell
idf.py build
```

To flash the firmware:

```powershell
idf.py flash
```

To monitor the serial output:

```powershell
idf.py monitor
```

Or combine the operations:

```powershell
idf.py build flash monitor
```

---

## Expected Runtime Behavior

After startup, the firmware performs the following sequence:

```text
ESP32 Boot
    │
    ▼
Initialize NVS
    │
    ▼
Initialize OLED
    │
    ▼
Initialize Sensors
    │
    ▼
Create Sensor Tasks
    │
    ▼
Initialize Wi-Fi
    │
    ▼
Connect to Access Point
    │
    ▼
Receive IP Address
    │
    ▼
Start HTTP Server
    │
    ▼
Acquire Sensor Data
    │
    ▼
Update Application State
    │
    ├───────────────┐
    ▼               ▼
 Update OLED    HTTP GET /api/sensors
                    │
                    ▼
                JSON Response
```

---

## Notes

The HTTP interface is intentionally simple.

The `/api/sensors` endpoint provides the current sensor state in JSON format, allowing external applications or web interfaces to retrieve the data without requiring a persistent connection.

This makes the firmware easier to maintain, test, and extend with additional sensors or communication endpoints in the future.
