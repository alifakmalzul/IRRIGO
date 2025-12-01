#include "esp32-dht11.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rom/ets_sys.h"
#include "esp_log.h"

static const char *TAG = "DHT11";

static inline void delay_us(uint32_t us) {
    ets_delay_us(us);
}

static esp_err_t wait_for_state(gpio_num_t gpio_num, int state, uint32_t timeout_us) {
    uint32_t elapsed = 0;
    while (gpio_get_level(gpio_num) != state) {
        if (elapsed > timeout_us) {
            return ESP_ERR_TIMEOUT;
        }
        delay_us(1);
        elapsed++;
    }
    return ESP_OK;
}

esp_err_t dht11_read(gpio_num_t gpio_num, dht11_reading_t *reading) {
    if (reading == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t data[5] = {0};
    
    // Configure GPIO as output with pull-up
    gpio_set_direction(gpio_num, GPIO_MODE_OUTPUT);
    gpio_set_pull_mode(gpio_num, GPIO_PULLUP_ONLY);
    
    // Send start signal
    gpio_set_level(gpio_num, 0);
    vTaskDelay(pdMS_TO_TICKS(20)); // 20ms low
    
    // Release the bus and switch to input
    gpio_set_level(gpio_num, 1);
    gpio_set_direction(gpio_num, GPIO_MODE_INPUT);
    delay_us(40); // 40us high
    
    // Wait for sensor response (sensor pulls LOW)
    if (wait_for_state(gpio_num, 0, 100) != ESP_OK) {
        ESP_LOGE(TAG, "Timeout waiting for sensor response (low)");
        return ESP_FAIL;
    }
    
    // Wait for sensor to pull HIGH
    if (wait_for_state(gpio_num, 1, 100) != ESP_OK) {
        ESP_LOGE(TAG, "Timeout waiting for sensor response (high)");
        return ESP_FAIL;
    }
    
    // Wait for sensor to pull LOW again (data start)
    if (wait_for_state(gpio_num, 0, 100) != ESP_OK) {
        ESP_LOGE(TAG, "Timeout waiting for data start");
        return ESP_FAIL;
    }
    
    // Read 40 bits of data
    for (int i = 0; i < 40; i++) {
        // Wait for bit start (high)
        if (wait_for_state(gpio_num, 1, 100) != ESP_OK) {
            ESP_LOGE(TAG, "Timeout waiting for bit %d start", i);
            return ESP_FAIL;
        }
        
        // Measure high pulse duration to determine bit value
        // Wait 40us then check: if still HIGH = '1' (70us), if LOW = '0' (26-28us)
        delay_us(40);
        int bit_value = gpio_get_level(gpio_num);
        
        // Wait for bit end (low)
        if (wait_for_state(gpio_num, 0, 100) != ESP_OK) {
            ESP_LOGE(TAG, "Timeout waiting for bit %d end", i);
            return ESP_FAIL;
        }
        
        // Store bit
        data[i / 8] <<= 1;
        if (bit_value) {
            data[i / 8] |= 1;
        }
    }
    
    // Verify checksum
    uint8_t checksum = data[0] + data[1] + data[2] + data[3];
    if (checksum != data[4]) {
        ESP_LOGE(TAG, "Checksum failed: calculated 0x%02X, received 0x%02X", checksum, data[4]);
        return ESP_FAIL;
    }
    
    // Parse data
    reading->humidity = (float)data[0] + (float)data[1] / 10.0f;
    reading->temperature = (float)data[2] + (float)data[3] / 10.0f;
    
    ESP_LOGD(TAG, "Temperature: %.1f°C, Humidity: %.1f%%", reading->temperature, reading->humidity);
    
    return ESP_OK;
}
