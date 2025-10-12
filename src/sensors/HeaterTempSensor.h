#ifndef DS18B20_SENSOR_H
#define DS18B20_SENSOR_H

#include "../interfaces/IHeaterTempSensor.h"
#include <OneWire.h>
#include <DallasTemperature.h>

/**
 * DS18B20Sensor - Heater temperature sensor implementation
 *
 * Wraps DS18B20 OneWire temperature sensor with error handling,
 * validation, and async reading support.
 *
 * Async Pattern:
 * 1. Call requestConversion() - starts conversion, returns immediately
 * 2. Wait ~750ms (for 12-bit resolution)
 * 3. Call isConversionReady() - returns true when ready
 * 4. Call read() - retrieves and validates the result
 */
class HeaterTempSensor : public IHeaterTempSensor {
private:
    OneWire oneWire;
    DallasTemperature sensor;
    float lastTemperature;
    bool valid;
    String lastError;
    uint8_t consecutiveErrors;

    // Async conversion tracking
    enum class ConversionState {
        IDLE,
        REQUESTED,
        READY
    };

    ConversionState conversionState;
    uint32_t conversionRequestTime;

    static constexpr uint8_t MAX_CONSECUTIVE_ERRORS = 3;
    static constexpr float MIN_VALID_TEMP = -50.0;
    static constexpr float MAX_VALID_TEMP = 150.0;
    static constexpr uint32_t CONVERSION_TIME_MS = 750;  // 12-bit resolution

    bool validateAndStoreReading(float temp) {
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
        conversionState = ConversionState::IDLE;
        return true;
    }

public:
    HeaterTempSensor(uint8_t pin)
        : oneWire(pin),
          sensor(&oneWire),
          lastTemperature(0.0),
          valid(false),
          consecutiveErrors(0),
          conversionState(ConversionState::IDLE),
          conversionRequestTime(0) {
    }

    void begin() override {
        sensor.begin();
        sensor.setResolution(12);  // 12-bit resolution (0.0625°C)
        sensor.setWaitForConversion(false);  // Async mode
        conversionState = ConversionState::IDLE;
    }

    void requestConversion() override {
        sensor.requestTemperatures();
        conversionState = ConversionState::REQUESTED;
        conversionRequestTime = millis();
    }

    bool isConversionReady() override {
        if (conversionState == ConversionState::IDLE) {
            return false;  // No conversion requested
        }

        if (conversionState == ConversionState::READY) {
            return true;  // Already marked as ready
        }

        // Check if enough time has passed
        if (millis() - conversionRequestTime >= CONVERSION_TIME_MS) {
            conversionState = ConversionState::READY;
            return true;
        }

        return false;
    }

    bool read() override {
        // If using async pattern, conversion should already be ready
        if (conversionState == ConversionState::REQUESTED) {
            // Force wait if not ready yet (for backward compatibility)
            while (!isConversionReady()) {
                delay(10);
            }
        } else if (conversionState == ConversionState::IDLE) {
            // Synchronous mode: request and wait
            sensor.requestTemperatures();
            delay(CONVERSION_TIME_MS);
        }

        // Read temperature
        float temp = sensor.getTempCByIndex(0);
        return validateAndStoreReading(temp);
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