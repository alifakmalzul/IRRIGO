# Smart Plant Watering System - Project Documentation

## Table of Contents
1. [Project Overview](#project-overview)
2. [System Architecture](#system-architecture)
3. [Hardware Components](#hardware-components)
4. [Software Design](#software-design)
5. [Features and Functionality](#features-and-functionality)
6. [Task Description](#task-description)
7. [ESP RainMaker Integration](#esp-rainmaker-integration)
8. [GPIO Pin Configuration](#gpio-pin-configuration)
9. [Installation and Setup](#installation-and-setup)
10. [Usage Guide](#usage-guide)
11. [Code Structure](#code-structure)
12. [Troubleshooting](#troubleshooting)

---

## Project Overview

The **Smart Plant Watering System** is an IoT-based automated watering solution built on the **ESP32-C3** microcontroller using the **ESP-IDF** framework and **ESP RainMaker** cloud platform. The system intelligently monitors environmental conditions (soil moisture, temperature, and humidity) and automatically waters plants based on multiple trigger mechanisms.

### Key Capabilities
- Automated watering based on soil moisture levels
- Scheduled watering using configurable time slots
- Manual control via physical pushbutton
- Voice assistant integration (Amazon Alexa / Google Assistant)
- Remote monitoring and control via ESP RainMaker mobile app
- Real-time sensor data reporting
- OTA (Over-The-Air) firmware updates
- Multi-tasking using FreeRTOS

---

## System Architecture

### High-Level Architecture
```
┌─────────────────────────────────────────────────────────┐
│                   ESP RainMaker Cloud                   │
│          (Remote Monitoring & Control)                  │
└────────────────┬────────────────────────────────────────┘
                 │ Wi-Fi
                 │
┌────────────────▼────────────────────────────────────────┐
│              ESP32-C3 Microcontroller                   │
│  ┌──────────────────────────────────────────────────┐  │
│  │           FreeRTOS Task Scheduler                │  │
│  │  ┌──────────┬───────────┬──────────┬──────────┐ │  │
│  │  │ Sensor   │ t1_drysoil│t2_schedule│t3_push   │ │  │
│  │  │ Task     │  Task     │  Task    │button    │ │  │
│  │  └──────────┴───────────┴──────────┴──────────┘ │  │
│  └──────────────────────────────────────────────────┘  │
│         ▲          ▲           ▲           ▲           │
└─────────┼──────────┼───────────┼───────────┼───────────┘
          │          │           │           │
    ┌─────┴────┐ ┌───┴────┐  ┌──┴────┐  ┌───┴────┐
    │  DHT11   │ │ Soil   │  │ Relay │  │ Push   │
    │  Sensor  │ │Moisture│  │Module │  │Button  │
    └──────────┘ └────────┘  └───┬───┘  └────────┘
                                  │
                             ┌────▼────┐
                             │  Water  │
                             │  Pump   │
                             └─────────┘
```

### Communication Flow
1. **Sensor Monitoring**: Continuous reading of environmental data
2. **Decision Logic**: Determines if watering is needed
3. **Actuation**: Controls water pump via relay
4. **Cloud Sync**: Updates status to ESP RainMaker
5. **User Interface**: Mobile app / Voice assistant feedback

---

## Hardware Components

| Component | Model/Type | GPIO Pin | Purpose |
|-----------|-----------|----------|---------|
| **Microcontroller** | Seeed Studio ESP32-C3 | - | Main processing unit |
| **Soil Moisture Sensor** | Capacitive/Resistive | ADC1_CH4 (GPIO4) | Measures soil moisture |
| **Temperature & Humidity** | DHT11 | GPIO3 | Environmental monitoring |
| **Relay Module** | 5V Single Channel | GPIO2 | Controls water pump |
| **Water Pump** | DC Submersible | - | Waters the plant |
| **Pushbutton** | Momentary Switch | GPIO5 | Manual watering trigger |
| **Power Supply** | 5V USB / Battery | - | Powers the system |

### Wiring Diagram
```
ESP32-C3               Components
────────               ──────────
GPIO2  ────────────►  Relay IN (Water Pump Control)
GPIO3  ────────────►  DHT11 Data Pin
GPIO4  ────────────►  Soil Moisture Sensor (Analog)
GPIO5  ────────────►  Pushbutton (Active Low)
GND    ────────────►  Common Ground
3.3V   ────────────►  Sensor Power (DHT11)
5V     ────────────►  Relay & Pump Power
```

---

## Software Design

### Technology Stack
- **Framework**: ESP-IDF v5.x
- **RTOS**: FreeRTOS
- **Cloud Platform**: ESP RainMaker
- **Programming Language**: C
- **Build System**: CMake

### Key Libraries and Components
- `esp_rmaker_core.h` - RainMaker core functionality
- `freertos/FreeRTOS.h` - Real-time operating system
- `esp_adc/adc_oneshot.h` - ADC for soil moisture reading
- `esp32-dht11.h` - DHT11 temperature/humidity sensor
- `driver/gpio.h` - GPIO control
- `esp_rmaker_ota.h` - Over-the-air updates

---

## Features and Functionality

### 1. Automatic Watering (Soil Moisture Based)
- **Trigger**: Soil moisture reading > 2700 (dry soil)
- **Action**: Activates water pump for 2 seconds
- **Notification**: Sends alert to RainMaker app
- **Task**: `t1_drysoil`

### 2. Scheduled Watering
- **Trigger**: Time-based schedule (configurable via app)
- **Default Schedule**: 08:00, 12:00, 16:00
- **Format**: HH:MM (24-hour format, comma-separated)
- **Action**: Waters at specified times
- **Task**: `t2_schedule`

### 3. Manual Watering (Pushbutton)
- **Trigger**: Physical button press (GPIO5 LOW)
- **Action**: Immediate watering for 2 seconds
- **Use Case**: On-demand watering without app/internet
- **Task**: `t3_pushbutton`

### 4. Voice Control
- **Integration**: Amazon Alexa / Google Assistant
- **Commands**: "Turn on/off water pump"
- **Implementation**: ESP RainMaker voice integration
- **Callback**: `voice_control_callback()`

### 5. Remote Monitoring
- **Platform**: ESP RainMaker mobile app
- **Parameters Monitored**:
  - Temperature (°C)
  - Humidity (%)
  - Soil Moisture State (Dry/OK/Wet)
  - Alert Messages
- **Update Interval**: 30 seconds

### 6. OTA Updates
- **Method**: ESP RainMaker OTA service
- **Trigger**: Remote firmware push
- **Benefit**: Update without physical access

---

## Task Description

The system uses FreeRTOS multitasking to handle concurrent operations efficiently.

### Task 1: Sensor Task (`sensor_task`)
**Priority**: 5  
**Stack Size**: 4096 bytes  
**Period**: 30 seconds

**Responsibilities**:
- Reads DHT11 temperature and humidity
- Reads soil moisture via ADC
- Determines soil state (Dry/OK/Wet)
- Updates RainMaker parameters
- Triggers `t1_drysoil` task when soil is dry
- Sends notifications to queue

**Soil Moisture Thresholds**:
```c
if (raw_value > 2700)       → Dry (trigger watering)
else if (raw_value > 1500)  → OK (normal)
else                        → Wet (too much water)
```

### Task 2: Dry Soil Watering Task (`t1_drysoil`)
**Priority**: 5  
**Stack Size**: 4096 bytes  
**Trigger**: Task notification from sensor_task

**Responsibilities**:
- Waits for notification from sensor task
- Acquires watering mutex (prevents simultaneous watering)
- Activates relay (GPIO2 = HIGH)
- Waters for 2 seconds
- Deactivates relay
- Releases mutex

**Code Flow**:
```c
Wait for notification → Acquire mutex → Water (2s) → Release mutex
```

### Task 3: Scheduled Watering Task (`t2_schedule`)
**Priority**: 5  
**Stack Size**: 4096 bytes  
**Period**: 1 second (checks time)

**Responsibilities**:
- Reads watering schedule from RainMaker parameter
- Parses comma-separated time values (HH:MM)
- Compares current time with schedule
- Executes watering at matching times
- Sends notification on scheduled watering

**Schedule Format**:
```
"08:00,12:00,16:00" → Waters at 8 AM, 12 PM, and 4 PM
```

### Task 4: Pushbutton Watering Task (`t3_pushbutton`)
**Priority**: 5  
**Stack Size**: 4096 bytes  
**Period**: 1 second (polls button)

**Responsibilities**:
- Monitors GPIO5 button state
- Detects button press (LOW signal)
- Acquires mutex and waters immediately
- Provides manual override capability

### Task 5: Notification Task (`notification_task`)
**Priority**: 5  
**Stack Size**: 4096 bytes  
**Trigger**: Queue messages

**Responsibilities**:
- Receives notifications from queue
- Sends alerts to ESP RainMaker
- Provides user feedback on events

### Task Synchronization
- **Mutex**: `watering_mutex` - Prevents concurrent watering operations
- **Queue**: `notification_queue` - Inter-task communication for alerts
- **Task Notification**: Direct notification from sensor to watering task

---

## ESP RainMaker Integration

### RainMaker Node Configuration
```c
Node Name: "Plant Watering System"
Node Type: "Test Notifications"
```

### Device 1: Plant Status (Temperature Sensor)
**Parameters**:
- `Temperature Value` (float, read-only)
- `Humidity Value` (float, read-only)
- `Soil Moisture State` (string, read-only)
- `Alert` (string, read-only)

### Device 2: Water Pump (Switch)
**Parameters**:
- `Switch` (boolean, read-write) - Voice/app control
- `Watering Schedule` (string, read-write) - Time configuration

### RainMaker Features Enabled
- Time synchronization
- OTA updates
- App insights
- Voice assistant integration

---

## GPIO Pin Configuration

| GPIO | Direction | Function | Active Level |
|------|-----------|----------|--------------|
| GPIO2 | Output | Relay control | HIGH (ON) / LOW (OFF) |
| GPIO3 | Input/Output | DHT11 data | Digital communication |
| GPIO4 | Input (ADC) | Soil moisture analog | 0-4095 (12-bit) |
| GPIO5 | Input | Pushbutton | LOW (pressed) |

### ADC Configuration
- **ADC Unit**: ADC1
- **Channel**: ADC1_CHANNEL_4 (GPIO4)
- **Resolution**: 12-bit (0-4095)
- **Attenuation**: Default
- **Mode**: One-shot reading

---

## Installation and Setup

### Prerequisites
1. **ESP-IDF**: Version 5.x or later
2. **ESP RainMaker**: Installed via `idf.py add-dependency`
3. **Python**: 3.7 or higher (for ESP-IDF tools)
4. **ESP RainMaker App**: Download from App Store / Play Store

### Build Instructions

1. **Clone or navigate to project directory**:
   ```bash
   cd c:\Users\Alif\waterproj
   ```

2. **Set ESP32-C3 as target**:
   ```bash
   idf.py set-target esp32c3
   ```

3. **Configure project** (optional):
   ```bash
   idf.py menuconfig
   ```

4. **Build the project**:
   ```bash
   idf.py build
   ```

5. **Flash to ESP32-C3**:
   ```bash
   idf.py -p COM_PORT flash
   ```

6. **Monitor serial output**:
   ```bash
   idf.py -p COM_PORT monitor
   ```

### Wi-Fi Provisioning

1. **Flash and power on the device**
2. **Open ESP RainMaker app**
3. **Tap "Add Device"**
4. **Scan QR code** (displayed in serial monitor)
5. **Connect to device's Wi-Fi AP**
6. **Enter your home Wi-Fi credentials**
7. **Wait for provisioning to complete**

### Device Setup in RainMaker

After provisioning:
1. Device appears as "Plant Watering System"
2. Configure watering schedule via "Watering Schedule" parameter
3. Monitor temperature, humidity, and soil moisture in real-time
4. Control water pump via toggle switch
5. Link to Alexa/Google Home for voice control

---

## Usage Guide

### Normal Operation

1. **Power on the system** - ESP32 boots and connects to Wi-Fi
2. **Automatic monitoring** - Sensors read every 30 seconds
3. **Auto-watering triggers**:
   - Dry soil detected → Waters automatically
   - Scheduled time reached → Waters per schedule
   - Button pressed → Waters immediately

### Changing Watering Schedule

**Via Mobile App**:
1. Open ESP RainMaker app
2. Select "Water Pump" device
3. Tap "Watering Schedule"
4. Enter times in format: `HH:MM,HH:MM,HH:MM`
5. Example: `06:30,18:00` (waters at 6:30 AM and 6:00 PM)

### Voice Control

**Alexa Commands**:
- "Alexa, turn on water pump"
- "Alexa, turn off water pump"

**Google Assistant**:
- "Ok Google, turn on water pump"
- "Ok Google, turn off water pump"

### Manual Override

Press the physical pushbutton to water immediately, regardless of soil state or schedule.

### Monitoring Soil Health

**Soil State Indicators**:
- **Dry** (> 2700): Red alert, auto-watering activated
- **OK** (1500-2700): Green status, no action needed
- **Wet** (< 1500): Blue warning, may indicate overwatering or drainage issue

---

## Code Structure

### Main Files

```
waterproj/
├── main/
│   ├── app_main.c          # Main application logic
│   ├── app_driver.c        # GPIO driver functions
│   ├── app_priv.h          # Private header definitions
│   ├── CMakeLists.txt      # Main component build config
│   └── idf_component.yml   # Component dependencies
├── components/
│   └── esp32-dht11/        # DHT11 sensor library
├── CMakeLists.txt          # Project-level CMake
├── sdkconfig               # ESP-IDF configuration
├── partitions.csv          # Flash partition table
└── README.md               # Project readme
```

### Key Functions

| Function | Purpose |
|----------|---------|
| `app_main()` | Entry point, initializes system |
| `sensor_task()` | Reads sensors and monitors conditions |
| `t1_drysoil()` | Handles dry soil watering |
| `t2_schedule()` | Executes scheduled watering |
| `t3_pushbutton()` | Handles manual button watering |
| `notification_task()` | Manages RainMaker notifications |
| `voice_control_callback()` | Processes voice commands |
| `write_cb()` | Handles RainMaker write requests |

### Configuration Macros

```c
#define SENSOR_ADC_CHANNEL  ADC_CHANNEL_4  // Soil moisture pin
#define CONFIG_DHT11_PIN    GPIO_NUM_3     // DHT11 data pin
#define RELAY_GPIO          GPIO_NUM_2     // Relay control pin
#define BUTTON_PIN          GPIO_NUM_5     // Manual button pin
#define MAX_WATERING_TIMES  5              // Max schedule entries
```

---

## Troubleshooting

### Problem: Device not connecting to Wi-Fi

**Solutions**:
- Verify Wi-Fi credentials during provisioning
- Check router supports 2.4GHz (ESP32-C3 doesn't support 5GHz)
- Reset device and re-provision
- Check serial monitor for error messages

### Problem: Soil moisture readings inconsistent

**Solutions**:
- Ensure sensor is properly inserted in soil
- Check sensor wiring and power supply
- Calibrate threshold values in code if needed
- Clean sensor contacts (corrosion can affect readings)

### Problem: Water pump not activating

**Solutions**:
- Verify relay module connections
- Check GPIO2 output with multimeter
- Ensure pump power supply is adequate (5V)
- Test relay independently
- Check mutex is not blocking operation

### Problem: DHT11 read failures

**Solutions**:
- Verify GPIO3 connection
- Check DHT11 power (3.3V)
- Increase timeout: `CONFIG_CONNECTION_TIMEOUT`
- Replace sensor if consistently failing
- Add pull-up resistor (4.7kΩ) on data line

### Problem: Scheduled watering not working

**Solutions**:
- Verify time synchronization in RainMaker config
- Check schedule format: `HH:MM,HH:MM`
- Ensure device has internet connection
- Review serial logs for time parsing errors
- Verify NTP server is accessible

### Problem: Voice control not responding

**Solutions**:
- Re-link account in Alexa/Google Home app
- Check RainMaker integration is active
- Verify device is online in RainMaker app
- Try re-discovering devices in voice assistant app
- Check pump parameter permissions (read-write)

### Problem: OTA update fails

**Solutions**:
- Ensure stable Wi-Fi connection
- Check sufficient flash space
- Verify OTA is enabled: `esp_rmaker_ota_enable_default()`
- Review serial logs for specific error codes
- Try smaller firmware updates

---

## Advanced Configuration

### Adjusting Watering Duration

Modify the delay in watering tasks:
```c
vTaskDelay(pdMS_TO_TICKS(2000));  // 2 seconds default
// Change to desired duration (in milliseconds)
```

### Customizing Sensor Reading Interval

In `sensor_task()`:
```c
vTaskDelay(pdMS_TO_TICKS(30000));  // 30 seconds default
// Adjust to desired interval
```

### Modifying Soil Moisture Thresholds

Calibrate based on your sensor and soil type:
```c
if (raw_value > 2700)      // Dry threshold
else if (raw_value > 1500) // Wet threshold
```

### Adding More Schedule Slots

Change the maximum:
```c
#define MAX_WATERING_TIMES 5  // Increase as needed
```

---

## Safety Considerations

1. **Electrical Safety**:
   - Use proper insulation for outdoor installations
   - Keep electronics away from water exposure
   - Use appropriate power ratings for pump

2. **Water Management**:
   - Monitor for overwatering (wet soil alerts)
   - Ensure drainage to prevent root rot
   - Use mutex to prevent simultaneous watering

3. **System Reliability**:
   - Implement watchdog timer (ESP Task WDT)
   - Handle sensor failures gracefully
   - Provide manual override capability

---

## Future Enhancements

Potential improvements for the system:

1. **Advanced Scheduling**: Support for different schedules per day of week
2. **Weather Integration**: Adjust watering based on weather forecast API
3. **Multiple Zones**: Control multiple plants/pumps independently
4. **Water Level Monitoring**: Detect when water reservoir is empty
5. **Data Logging**: Store historical sensor data for analytics
6. **Machine Learning**: Predict optimal watering times based on plant health
7. **Solar Power**: Battery + solar panel for off-grid operation
8. **Local Dashboard**: Web server on ESP32 for LAN-based control

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.4 | 2025 | Current version with three watering tasks |
| 1.0 | 2025 | Initial release |

---

## License

This project is provided as-is for educational purposes.

---

## Support

For technical issues or questions:
- Check ESP-IDF documentation: https://docs.espressif.com/
- ESP RainMaker docs: https://rainmaker.espressif.com/
- ESP32 forums: https://esp32.com/

---

## Conclusion

This Smart Plant Watering System demonstrates a complete IoT solution combining embedded systems, cloud connectivity, and home automation. The multi-task architecture ensures reliable operation while the ESP RainMaker integration provides seamless remote control and monitoring capabilities. The system's flexibility allows for various watering triggers, making it suitable for different plant types and environmental conditions.

**Project Developed For**: EEM 5043 - Embedded Systems Course  
**Academic Year**: 2025
