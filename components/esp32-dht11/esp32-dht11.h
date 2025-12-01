#ifndef ESP32_DHT11_H
#define ESP32_DHT11_H

#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float temperature;
    float humidity;
} dht11_reading_t;

/**
 * @brief Read temperature and humidity from DHT11 sensor
 * 
 * @param gpio_num GPIO pin number connected to DHT11 data pin
 * @param reading Pointer to store the reading
 * @return esp_err_t ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t dht11_read(gpio_num_t gpio_num, dht11_reading_t *reading);

#ifdef __cplusplus
}
#endif

#endif // ESP32_DHT11_H
