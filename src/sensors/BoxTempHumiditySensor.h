#ifndef AM2320_SENSOR_H
#define AM2320_SENSOR_H

#include "../interfaces/IBoxTempHumiditySensor.h"
#include <Adafruit_AM2320.h>

/**
 * AM2320Sensor - Box temperature and humidity sensor implementation
 *
 * Wraps Adafruit AM2320 sensor with error handling and validation.
 */
class BoxTempHumiditySensor : public IBoxTempHumiditySensor {
private:
    Adafruit_AM2320 sensor;
    float lastTemperature;
    float lastHumidity;
    bool valid;
    String lastError;
    uint8_t consecutiveErrors;

    static constexpr uint8_t MAX_CONSECUTIVE_ERRORS = 3;
    static constexpr float MIN_VALID_TEMP = -40.0;
    static constexpr float MAX_VALID_TEMP = 80.0;
    static constexpr float MIN_VALID_HUMIDITY = 0.0;
    static constexpr float MAX_VALID_HUMIDITY = 100.0;

public:
    BoxTempHumiditySensor()
        : lastTemperature(0.0),
          lastHumidity(0.0),
          valid(false),
          consecutiveErrors(0) {
    }

    void begin() override {
        if (!sensor.begin()) {
            valid = false;
            lastError = "AM2320 initialization failed";
            consecutiveErrors = MAX_CONSECUTIVE_ERRORS;
        }
    }

    bool read() override {
        float temp = sensor.readTemperature();
        float humidity = sensor.readHumidity();

        // Check for NaN (sensor communication error)
        if (isnan(temp) || isnan(humidity)) {
            consecutiveErrors++;
            if (consecutiveErrors >= MAX_CONSECUTIVE_ERRORS) {
                valid = false;
                lastError = "AM2320 communication error";
            }
            return false;
        }

        // Validate temperature range
        if (temp < MIN_VALID_TEMP || temp > MAX_VALID_TEMP) {
            consecutiveErrors++;
            if (consecutiveErrors >= MAX_CONSECUTIVE_ERRORS) {
                valid = false;
                lastError = "AM2320 temperature out of range: " + String(temp);
            }
            return false;
        }

        // Validate humidity range
        if (humidity < MIN_VALID_HUMIDITY || humidity > MAX_VALID_HUMIDITY) {
            consecutiveErrors++;
            if (consecutiveErrors >= MAX_CONSECUTIVE_ERRORS) {
                valid = false;
                lastError = "AM2320 humidity out of range: " + String(humidity);
            }
            return false;
        }

        // Valid reading
        consecutiveErrors = 0;
        lastTemperature = temp;
        lastHumidity = humidity;
        valid = true;
        lastError = "";
        return true;
    }

    float getTemperature() const override {
        return lastTemperature;
    }

    float getHumidity() const override {
        return lastHumidity;
    }

    bool isValid() const override {
        return valid;
    }

    String getLastError() const override {
        return lastError;
    }
};

#endif