#ifndef MOCK_BOX_TEMP_HUMIDITY_SENSOR_H
#define MOCK_BOX_TEMP_HUMIDITY_SENSOR_H

#include "../../src/interfaces/IBoxTempHumiditySensor.h"

/**
 * MockBoxTempHumiditySensor - Test double for IBoxTempHumiditySensor
 *
 * Allows manual control of readings and error states for testing.
 */
class MockBoxTempHumiditySensor : public IBoxTempHumiditySensor {
private:
    float temperature;
    float humidity;
    bool valid;
    String lastError;
    bool initialized;
    uint32_t readCallCount;

public:
    MockBoxTempHumiditySensor()
        : temperature(25.0),
          humidity(50.0),
          valid(true),
          initialized(false),
          readCallCount(0) {
    }

    void begin() override {
        initialized = true;
    }

    bool read() override {
        readCallCount++;
        return valid;
    }

    float getTemperature() const override {
        return temperature;
    }

    float getHumidity() const override {
        return humidity;
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
    }

    void setHumidity(float hum) {
        humidity = hum;
    }

    void setReadings(float temp, float hum) {
        temperature = temp;
        humidity = hum;
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

    void resetCallCount() {
        readCallCount = 0;
    }
};

#endif