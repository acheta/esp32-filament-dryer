#ifndef DS18B20_SENSOR_H
#define DS18B20_SENSOR_H

#include "../interfaces/IHeaterTempSensor.h"
#include <OneWire.h>
#include <DallasTemperature.h>

/**
 * DS18B20Sensor - Heater temperature sensor implementation
 *
 * Wraps DS18B20 OneWire temperature sensor with error handling
 * and validation.
 */
class HeaterTempSensor : public IHeaterTempSensor {
private:
    OneWire oneWire;
    DallasTemperature sensor;
    float lastTemperature;
    bool valid;
    String lastError;
    uint8_t consecutiveErrors;

    static constexpr uint8_t MAX_CONSECUTIVE_ERRORS = 3;
    static constexpr float MIN_VALID_TEMP = -50.0;
    static constexpr float MAX_VALID_TEMP = 150.0;

public:
    HeaterTempSensor(uint8_t pin)
        : oneWire(pin),
          sensor(&oneWire),
          lastTemperature(0.0),
          valid(false),
          consecutiveErrors(0) {
    }

    void begin() override {
        sensor.begin();
        sensor.setResolution(12);  // 12-bit resolution (0.0625°C)
        sensor.setWaitForConversion(false);  // Async mode
    }

    bool read() override {
        sensor.requestTemperatures();
        float temp = sensor.getTempCByIndex(0);

        // Validate reading
        if (temp == DEVICE_DISCONNECTED_C) {
            consecutiveErrors++;
            if (consecutiveErrors >= MAX_CONSECUTIVE_ERRORS) {
                valid = false;
                lastError = "DS18B20 disconnected";
            }
            return false;
        }

        if (temp < MIN_VALID_TEMP || temp > MAX_VALID_TEMP) {
            consecutiveErrors++;
            if (consecutiveErrors >= MAX_CONSECUTIVE_ERRORS) {
                valid = false;
                lastError = "DS18B20 reading out of range: " + String(temp);
            }
            return false;
        }

        // Valid reading
        consecutiveErrors = 0;
        lastTemperature = temp;
        valid = true;
        lastError = "";
        return true;
    }

    float getTemperature() const override {
        return lastTemperature;
    }

    bool isValid() const override {
        return valid;
    }

    String getLastError() const override {
        return lastError;
    }
};

#endif