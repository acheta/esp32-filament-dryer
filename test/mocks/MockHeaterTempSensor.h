#ifndef MOCK_HEATER_TEMP_SENSOR_H
#define MOCK_HEATER_TEMP_SENSOR_H

#include "../../src/interfaces/IHeaterTempSensor.h"

/**
 * MockHeaterTempSensor - Test double for IHeaterTempSensor
 *
 * Allows manual control of readings and error states for testing.
 */
class MockHeaterTempSensor : public IHeaterTempSensor {
private:
    float temperature;
    bool valid;
    String lastError;
    bool initialized;
    uint32_t readCallCount;
    uint32_t requestConversionCallCount;
    bool conversionReady;

public:
    MockHeaterTempSensor()
        : temperature(25.0),
          valid(true),
          initialized(false),
          readCallCount(0),
          requestConversionCallCount(0),
          conversionReady(true) {
    }

    void begin() override {
        initialized = true;
    }

    bool read() override {
        readCallCount++;
        return valid;
    }

    void requestConversion() override {
        requestConversionCallCount++;
        conversionReady = true;  // Immediately ready in mock
    }

    bool isConversionReady() override {
        return conversionReady;
    }

    float getTemperature() const override {
        return temperature;
    }

    bool isValid() const override {
        return valid;
    }

    String getLastError() const override {
        return lastError;
    }

    // ==================== Test Helper Methods ====================

    void setTemperature(float temp) {
        temperature = temp;
        valid = true;
        lastError = "";
    }

    void setInvalid(const String& error) {
        valid = false;
        lastError = error;
    }

    void setValid() {
        valid = true;
        lastError = "";
    }

    bool isInitialized() const {
        return initialized;
    }

    uint32_t getReadCallCount() const {
        return readCallCount;
    }

    uint32_t getRequestConversionCallCount() const {
        return requestConversionCallCount;
    }

    void resetCallCount() {
        readCallCount = 0;
        requestConversionCallCount = 0;
    }

    void setConversionReady(bool ready) {
        conversionReady = ready;
    }
};

#endif