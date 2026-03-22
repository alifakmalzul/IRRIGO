# OBE-IBM 5013 JAN26 — Assignment 2: Detailed Answers
## Smart Plant Watering System (IRRIGO)

---

## Table of Contents
1. [IoT System Overview](#1-iot-system-overview)
2. [Hardware Components and Interfacing](#2-hardware-components-and-interfacing)
3. [FreeRTOS Multitasking Design](#3-freertos-multitasking-design)
4. [Sensor Data Acquisition](#4-sensor-data-acquisition)
5. [Cloud Platform Integration — ESP RainMaker](#5-cloud-platform-integration--esp-rainmaker)
6. [Inter-Task Communication and Synchronization](#6-inter-task-communication-and-synchronization)
7. [Voice Assistant and Remote Control](#7-voice-assistant-and-remote-control)
8. [OTA Firmware Updates](#8-ota-firmware-updates)
9. [Code Bug Analysis and Fixes](#9-code-bug-analysis-and-fixes)
10. [System Evaluation](#10-system-evaluation)

---

## 1. IoT System Overview

### What is the IRRIGO system?

IRRIGO is an **IoT-based Smart Plant Watering System** built on the **Seeed Studio ESP32-C3** microcontroller.  
It automates plant watering by reading soil moisture, temperature, and humidity, then deciding whether and when to activate a water pump — without any human intervention.

### High-Level Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                     ESP RainMaker Cloud                      │
│   (Remote Monitoring, Scheduling, Voice Assistant, OTA)      │
└───────────────────────┬──────────────────────────────────────┘
                        │ Wi-Fi (TLS/MQTT)
                        │
┌───────────────────────▼──────────────────────────────────────┐
│                  ESP32-C3 Microcontroller                     │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │                FreeRTOS Task Scheduler                  │ │
│  │  ┌──────────┬────────────┬─────────────┬─────────────┐ │ │
│  │  │ sensor_  │ t1_drysoil │ t2_schedule │ t3_push     │ │ │
│  │  │  task    │            │             │  button     │ │ │
│  │  └────┬─────┴──────┬─────┴──────┬──────┴──────┬──────┘ │ │
│  │       │            │            │             │         │ │
│  │  ┌────▼──────────────────────────────────────▼──────┐  │ │
│  │  │   notification_task  (FreeRTOS Queue consumer)   │  │ │
│  │  └──────────────────────────────────────────────────┘  │ │
│  └─────────────────────────────────────────────────────────┘ │
│          ▲               ▲              ▲            ▲        │
└──────────┼───────────────┼──────────────┼────────────┼────────┘
     ┌─────┴────┐    ┌─────┴────┐   ┌────┴────┐  ┌────┴────┐
     │  DHT11   │    │   Soil   │   │  Relay  │  │  Push   │
     │ (Temp/   │    │ Moisture │   │ Module  │  │ Button  │
     │ Humidity)│    │ Sensor   │   │  + Pump │  │ (GPIO5) │
     └──────────┘    └──────────┘   └─────────┘  └─────────┘
```

### Three Watering Trigger Mechanisms

| # | Trigger | Task | Condition |
|---|---------|------|-----------|
| 1 | Dry soil detected | `t1_drysoil` | ADC reading > 2700 |
| 2 | Scheduled time | `t2_schedule` | Current time matches HH:MM schedule |
| 3 | Button press | `t3_pushbutton` | GPIO5 reads LOW |
| 4 | Voice/App command | `voice_control_callback` | RainMaker write event |

---

## 2. Hardware Components and Interfacing

### Component List

| Component | Model | GPIO | Interface | Purpose |
|-----------|-------|------|-----------|---------|
| Microcontroller | Seeed ESP32-C3 | — | — | Central processing unit |
| Soil Moisture Sensor | Capacitive | GPIO4 (ADC1_CH4) | Analog (ADC) | Measures soil dryness |
| Temperature/Humidity | DHT11 | GPIO3 | 1-Wire digital | Environmental monitoring |
| Water Pump Relay | 5V Single-Channel | GPIO2 | Digital output | Controls pump on/off |
| Pushbutton | Momentary switch | GPIO5 | Digital input | Manual watering override |

### GPIO Pin Configuration

```
ESP32-C3 Pin       Direction    Connected To           Active Level
─────────────      ─────────    ────────────           ────────────
GPIO2 (RELAY)      Output       Relay IN pin           HIGH = Pump ON
GPIO3 (DHT11)      Bidirectional DHT11 Data            1-Wire protocol
GPIO4 (ADC)        Input (ADC)  Soil moisture sensor   0 – 4095 (12-bit)
GPIO5 (BUTTON)     Input        Pushbutton             LOW = pressed
```

### Soil Moisture ADC Reading Thresholds

```
ADC Value         Soil State    Action
──────────────    ──────────    ──────────────────────────────
> 2700            Dry           Auto-water + send alert
1500 – 2700       OK            No action; normal status update
< 1500            Wet           Warning notification only
```

The **capacitive soil moisture sensor** outputs a voltage that is inversely proportional to moisture content. The ESP32-C3 12-bit ADC (0–4095) converts this voltage. A dry reading approaches the high end because there is less capacitance.

---

## 3. FreeRTOS Multitasking Design

### Why FreeRTOS?

FreeRTOS is a real-time operating system kernel. It allows multiple **tasks** (threads) to run concurrently on a single CPU core, each responding to its own trigger without blocking others. This is essential for IRRIGO because soil monitoring, schedule checking, button polling, and cloud notifications all need to happen independently.

### Task Summary Table

| Task Name | Priority | Stack | Period / Trigger | Responsibility |
|-----------|----------|-------|------------------|----------------|
| `sensor_task` | 5 | 4096 B | 30 s periodic | Read DHT11 + ADC, classify soil, notify cloud |
| `t1_drysoil` | 5 | 4096 B | Task notification | Activate pump when soil is dry |
| `t2_schedule` | 5 | 4096 B | 1 s periodic (time check) | Activate pump at configured times |
| `t3_pushbutton` | 5 | 4096 B | 1 s periodic (poll) | Activate pump on button press |
| `notification_task` | 5 | 4096 B | Queue-driven | Send RainMaker push alerts |

### Task Lifecycle Diagram

```
app_main()
    │
    ├─ xTaskCreate(sensor_task)   ──► reads sensors every 30 s
    │                                     │
    │                                     │ if soil is Dry
    │                                     ▼
    ├─ xTaskCreate(t1_drysoil)  ◄── xTaskNotifyGive()
    │                                waits → takes mutex → waters 2 s → gives mutex
    │
    ├─ xTaskCreate(t2_schedule)  ──► every 1 s: compare time → waters if match
    │
    ├─ xTaskCreate(t3_pushbutton) ─► every 1 s: check GPIO5 → waters if pressed
    │
    └─ xTaskCreate(notification_task) ◄── xQueueReceive() → sends RainMaker alert
```

### Task Notification (sensor → t1_drysoil)

`xTaskNotifyGive()` sends a lightweight integer notification directly from `sensor_task` to `t1_drysoil`. This is faster and more memory-efficient than a semaphore or queue when only a single "trigger" signal is needed.

```c
// In sensor_task: signal the watering task
if (receiverTaskHandle != NULL) {
    xTaskNotifyGive(receiverTaskHandle);
}

// In t1_drysoil: block until signal arrives
ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
```

---

## 4. Sensor Data Acquisition

### DHT11 (Temperature and Humidity)

The DHT11 uses a proprietary single-wire serial protocol. The custom `esp32-dht11` component bit-bangs the GPIO line:

1. MCU pulls line LOW for ≥18 ms (start signal)
2. DHT11 responds with LOW (80 µs) then HIGH (80 µs)
3. 40 bits of data follow: 8-bit humidity integer, 8-bit humidity decimal, 8-bit temperature integer, 8-bit temperature decimal, 8-bit checksum

```c
dht11_reading_t dht11_reading;
if (dht11_read(CONFIG_DHT11_PIN, &dht11_reading) == ESP_OK) {
    temperature = dht11_reading.temperature;   // °C
    humidity    = dht11_reading.humidity;      // %
}
```

### Soil Moisture Sensor (ADC)

The ESP-IDF `adc_oneshot` driver is used for single-sample analog reads:

```c
// Initialise ADC unit and channel once (in sensor_task before the loop)
adc_oneshot_unit_init_cfg_t adc1_init_config = {
    .unit_id  = ADC_UNIT_1,
    .ulp_mode = ADC_ULP_MODE_DISABLE,
};
adc_oneshot_new_unit(&adc1_init_config, &adc1_handle);

adc_oneshot_chan_cfg_t adc_channel_config = {
    .atten    = ADC_ATTEN_DB_12,     // full-scale ≈ 3.1 V on 3.3 V supply
    .bitwidth = ADC_BITWIDTH_DEFAULT, // 12-bit → 0–4095
};
adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_4, &adc_channel_config);

// Inside the loop
adc_oneshot_read(adc1_handle, ADC_CHANNEL_4, &raw_value);
```

---

## 5. Cloud Platform Integration — ESP RainMaker

### What is ESP RainMaker?

ESP RainMaker is Espressif's serverless IoT cloud platform. It provides:
- **Device provisioning** via QR code + mobile app
- **Remote parameter read/write** (MQTT over TLS)
- **Push notifications** via `esp_rmaker_raise_alert()`
- **Voice assistant integration** (Alexa, Google Assistant)
- **OTA firmware updates**
- **NTP time synchronisation** (required for scheduled watering)

### Node and Device Model

```
RainMaker Node: "Plant Watering System"
│
├── Device: "Plant Status" (type: Temperature Sensor)
│   ├── Parameter: "Temperature Value"  (float, read-only)
│   ├── Parameter: "Humidity Value"     (float, read-only)
│   ├── Parameter: "Soil Moisture State"(string, read-only)
│   └── Parameter: "Alert"             (string, read-only)
│
└── Device: "Water Pump" (type: Switch)
    ├── Parameter: "Power"             (bool, read-write, toggle UI)
    └── Parameter: "Watering Schedule"(string, read-write)
```

### Callback: `voice_control_callback`

This single callback handles **both** the `Power` toggle **and** `Watering Schedule` updates, because a device can have only one write callback in RainMaker:

```c
static esp_err_t voice_control_callback(...) {
    // Watering schedule write
    if (param == watering_schedule_param) {
        // parse "HH:MM,HH:MM,..." with strtok_r (reentrant)
        ...
        esp_rmaker_param_update(param, val);
        return ESP_OK;
    }
    // Pump power toggle
    if (val.type == RMAKER_VAL_TYPE_BOOLEAN) {
        xSemaphoreTake(watering_mutex, pdMS_TO_TICKS(1000));
        gpio_set_level(RELAY_GPIO, val.val.b ? 1 : 0);
        esp_rmaker_param_update(param, val);
        xQueueSend(notification_queue, &msg, portMAX_DELAY);
        xSemaphoreGive(watering_mutex);
        return ESP_OK;
    }
    ...
}
```

### Reporting Sensor Data

```c
esp_rmaker_param_update_and_report(temperature_value_param, esp_rmaker_float(temperature));
esp_rmaker_param_update_and_report(humidity_value_param,    esp_rmaker_float(humidity));
esp_rmaker_param_update_and_report(soil_moisture_state_param, esp_rmaker_str(soil_state));
```

`update_and_report` both caches the value locally and immediately publishes it to the cloud.

---

## 6. Inter-Task Communication and Synchronization

### Problem: Race Conditions on the Relay GPIO

Three independent tasks (`t1_drysoil`, `t2_schedule`, `t3_pushbutton`) and the `voice_control_callback` can all try to activate the pump simultaneously. Without synchronisation, GPIO2 could be set HIGH by one task and then immediately set LOW by another, or the relay could be driven in unexpected patterns.

### Solution: Mutex (`watering_mutex`)

A FreeRTOS **mutex** is a binary semaphore that ensures **mutual exclusion** — only one holder at a time:

```c
watering_mutex = xSemaphoreCreateMutex();

// In every watering code path:
if (xSemaphoreTake(watering_mutex, portMAX_DELAY)) {
    gpio_set_level(RELAY_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(2000));
    gpio_set_level(RELAY_GPIO, 0);
    xSemaphoreGive(watering_mutex);
}
```

`t2_schedule` uses a non-blocking `xSemaphoreTake(watering_mutex, 0)` — if the mutex is not immediately available the scheduled slot is skipped gracefully rather than blocking the 1-second polling loop.

### Notification Queue (`notification_queue`)

A FreeRTOS **queue** decouples the fast sensor/watering tasks from the slower RainMaker cloud API:

```
sensor_task / t2_schedule / voice_control_callback
        │
        │  xQueueSend(notification_queue, &msg, ...)
        ▼
 notification_task  ←  xQueueReceive(notification_queue, &msg, portMAX_DELAY)
        │
        └─► send_unified_notification()  →  esp_rmaker_raise_alert()
```

Queue capacity is **5 messages**, each holding a 64-character string. This absorbs short bursts of events without blocking the producers.

---

## 7. Voice Assistant and Remote Control

### Integration Architecture

```
User (Alexa / Google Assistant)
        │ "Turn on water pump"
        ▼
  Voice Platform (cloud)
        │  OAuth link via RainMaker
        ▼
  ESP RainMaker Cloud
        │  MQTT write → param "Power" = true
        ▼
  ESP32-C3 device
        │  voice_control_callback()
        ▼
  GPIO2 = HIGH  →  Relay closes  →  Pump ON
```

### Enabling Voice Integration

1. In `app_main()`: register `voice_control_callback` on the pump device.
2. In the RainMaker mobile app: link Alexa or Google Home account.
3. In Alexa/Google Home: discover new devices — the pump appears as a smart switch.

Commands:
- **Alexa**: "Alexa, turn on water pump" / "Alexa, turn off water pump"
- **Google**: "Ok Google, turn on water pump" / "Ok Google, turn off water pump"

---

## 8. OTA Firmware Updates

OTA (Over-The-Air) updates allow the firmware to be updated without physical access to the device:

```c
esp_rmaker_ota_enable_default();
```

This single call:
1. Registers the RainMaker OTA service
2. Listens for firmware push events from the RainMaker dashboard
3. Downloads the new binary over HTTPS
4. Validates it and writes it to the inactive flash partition
5. Reboots into the new firmware

The partition table (`partitions.csv`) reserves two OTA slots (`ota_0` and `ota_1`) so that if an update fails the device falls back to the previous firmware automatically.

---

## 9. Code Bug Analysis and Fixes

This section documents the five bugs found in the original firmware and explains why each fix is necessary.

### Bug 1 — `#include "app_driver.c"` (Build-Breaking)

**File**: `main/app_main.c`, original line 17  
**Problem**: Including a `.c` source file causes the same functions (`app_driver_init`, `app_driver_set_gpio`) to be compiled **twice** — once as part of `app_main.c` and once as their own translation unit (listed in `CMakeLists.txt`). The linker then sees duplicate symbols and fails with a **multiple definition error**.

**Fix**: Replace the `.c` include with the proper header:
```c
// Before (broken):
#include "app_driver.c"

// After (correct):
#include "app_priv.h"
```

---

### Bug 2 — Stale Template Declarations in `app_priv.h` (Compilation Error)

**File**: `main/app_priv.h`  
**Problem**: The header was copied from an ESP RainMaker light-bulb example. It declared `extern esp_rmaker_device_t *light_device` and functions like `app_light_set_power()` which do not exist in this project. This caused **undeclared symbol** errors at link time.

**Fix**: Remove all light-device boilerplate and declare only the functions that actually exist in `app_driver.c`:
```c
// After fix:
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>

void app_driver_init(void);
esp_err_t app_driver_set_gpio(const char *param_name, bool state);
```

---

### Bug 3 — `enable_time_sync = false` (Scheduled Watering Never Fires)

**File**: `main/app_main.c`, `rainmaker_cfg`  
**Problem**: `t2_schedule` calls `time()` and `localtime_r()` to compare the current wall-clock time against the watering schedule. With `enable_time_sync = false`, the ESP32-C3 never contacts the NTP server — its internal RTC starts from the epoch (1 Jan 1970) and never advances to the real time. Every comparison `timeinfo.tm_hour == hour` therefore always fails and the pump never activates on schedule.

**Fix**:
```c
esp_rmaker_config_t rainmaker_cfg = {
    .enable_time_sync = true,   // was false — NTP needed by t2_schedule
};
```

---

### Bug 4 — Schedule Callback Never Registered (Silent Data Drop)

**File**: `main/app_main.c`  
**Problem**: `watering_schedules_cb` was defined but `esp_rmaker_device_add_cb` was only called once with `voice_control_callback`. The RainMaker SDK routes all write events for a device through the single registered callback; a second callback function that is never registered is simply never invoked. Any schedule update sent from the mobile app was silently discarded.

**Additionally**, the original `watering_schedules_cb` used `strtok()` which is **not reentrant** — calling it from a FreeRTOS task while another task also calls `strtok` would corrupt the shared internal state pointer.

**Fix**: Merge the schedule-handling logic into `voice_control_callback`, using `strtok_r` (reentrant, takes a `saveptr` argument):
```c
if (param == watering_schedule_param) {
    char *saveptr = NULL;
    char *token = strtok_r(schedule_copy, ",", &saveptr);
    while (token != NULL && i < MAX_WATERING_TIMES) {
        ...
        token = strtok_r(NULL, ",", &saveptr);
    }
    esp_rmaker_param_update(param, val);
    return ESP_OK;
}
```

---

### Bug 5 — Parameter Name Mismatch in `app_driver_set_gpio` (GPIO Never Updated via write_cb)

**File**: `main/app_driver.c`  
**Problem**: `app_driver_set_gpio` checked `strcmp(param_name, "Pump")` but the RainMaker parameter for the pump was created with the name `"Power"`:
```c
pump_param = esp_rmaker_param_create("Power", ESP_RMAKER_PARAM_POWER, ...);
```
Because `"Power" != "Pump"`, `app_driver_set_gpio` always returned `ESP_FAIL` and `write_cb` never updated GPIO2.

**Fix**:
```c
// Before:
if (strcmp(param_name, "Pump") == 0) {

// After:
if (strcmp(param_name, "Power") == 0) {
```

---

## 10. System Evaluation

### Strengths

| Aspect | Detail |
|--------|--------|
| **Multi-trigger watering** | Three independent triggers (soil, schedule, button) ensure plants are watered even if one mechanism fails |
| **Mutex protection** | `watering_mutex` prevents concurrent relay activation, protecting the relay from conflicting drive states |
| **Queue-based notifications** | Decouples fast sensor tasks from slow cloud API calls, preventing task blocking |
| **OTA updates** | Firmware can be patched remotely without physical access |
| **Voice assistant support** | Low-friction user interaction via Alexa / Google Assistant |

### Limitations and Possible Improvements

| Limitation | Suggested Improvement |
|------------|-----------------------|
| Fixed 2-second watering duration | Make watering duration a configurable RainMaker parameter |
| No water level detection | Add a float sensor to detect empty reservoir and halt watering |
| Sensor readings every 30 s | Allow user to configure polling interval via app |
| Single-zone control | Add support for multiple GPIO outputs to water different plant zones |
| No historical data | Log sensor readings to NVS or an external database for trend analysis |
| `t2_schedule` time check fires once per second | Use an event-group bit set by an NTP sync callback to avoid acting before valid time is available |

### Security Considerations

- All communication between the ESP32-C3 and ESP RainMaker Cloud is over **TLS-encrypted MQTT**, preventing eavesdropping or relay attacks.
- Device identity is established during provisioning using unique per-device certificates stored in the NVS.
- OTA images are signed; the bootloader verifies the signature before accepting a new firmware image.

---

*End of Assignment 2 Answers*
