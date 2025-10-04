#ifndef MOCK_SENSOR_MANAGER_H
#define MOCK_SENSOR_MANAGER_H

#include "../../src/interfaces/ISensorManager.h"
#include <vector>

/**
 * MockSensorManager - Test double for ISensorManager
 *
 * Allows setting sensor values and triggering callbacks manually
 * for deterministic testing. Does NOT require actual sensor objects.
 */
class MockSensorManager : public ISensorManager {
private:
    SensorReading heaterTemp;
    SensorReading boxTemp;
    SensorReading boxHumidity;

    std::vector<HeaterTempCallback> heaterTempCallbacks;
    std::vector<BoxDataCallback> boxDataCallbacks;
    std::vector<SensorErrorCallback> errorCallbacks;

    bool initialized;
    uint32_t updateCallCount;

public:
    MockSensorManager()
        : initialized(false),
          updateCallCount(0) {
        heaterTemp.value = 25.0;
        heaterTemp.isValid = true;
        heaterTemp.timestamp = 0;

        boxTemp.value = 25.0;
        boxTemp.isValid = true;
        boxTemp.timestamp = 0;

        boxHumidity.value = 50.0;
        boxHumidity.isValid = true;
        boxHumidity.timestamp = 0;
    }

    void begin() override {
        initialized = true;
    }

    void update(uint32_t currentMillis) override {
        updateCallCount++;
        // Mock doesn't auto-update, use manual trigger methods
    }

    void registerHeaterTempCallback(HeaterTempCallback callback) override {
        heaterTempCallbacks.push_back(callback);
    }

    void registerBoxDataCallback(BoxDataCallback callback) override {
        boxDataCallbacks.push_back(callback);
    }

    void registerSensorErrorCallback(SensorErrorCallback callback) override {
        errorCallbacks.push_back(callback);
    }

    SensorReadings getReadings() const override {
        SensorReadings readings;
        readings.heaterTemp = heaterTemp;
        readings.boxTemp = boxTemp;
        readings.boxHumidity = boxHumidity;
        return readings;
    }

    float getHeaterTemp() const override {
        return heaterTemp.value;
    }

    float getBoxTemp() const override {
        return boxTemp.value;
    }

    float getBoxHumidity() const override {
        return boxHumidity.value;
    }

    bool isHeaterTempValid() const override {
        return heaterTemp.isValid;
    }

    bool isBoxDataValid() const override {
        return boxTemp.isValid && boxHumidity.isValid;
    }

    // ==================== Test Helper Methods ====================

    void setHeaterTemp(float temp, uint32_t timestamp = 0) {
        heaterTemp.value = temp;
        heaterTemp.timestamp = timestamp;
        heaterTemp.isValid = true;
    }

    void setBoxTemp(float temp, uint32_t timestamp = 0) {
        boxTemp.value = temp;
        boxTemp.timestamp = timestamp;
        boxTemp.isValid = true;
    }

    void setBoxHumidity(float humidity, uint32_t timestamp = 0) {
        boxHumidity.value = humidity;
        boxHumidity.timestamp = timestamp;
        boxHumidity.isValid = true;
    }

    void setHeaterTempInvalid() {
        heaterTemp.isValid = false;
    }

    void setBoxDataInvalid() {
        boxTemp.isValid = false;
        boxHumidity.isValid = false;
    }

    void triggerHeaterTempUpdate(float temp, uint32_t timestamp) {
        setHeaterTemp(temp, timestamp);
        for (auto& callback : heaterTempCallbacks) {
            if (callback) {
                callback(temp, timestamp);
            }
        }
    }

    void triggerBoxDataUpdate(float temp, float humidity, uint32_t timestamp) {
        setBoxTemp(temp, timestamp);
        setBoxHumidity(humidity, timestamp);
        for (auto& callback : boxDataCallbacks) {
            if (callback) {
                callback(temp, humidity, timestamp);
            }
        }
    }

    void triggerSensorError(SensorType type, const String& error) {
        if (type == SensorType::HEATER_TEMP) {
            setHeaterTempInvalid();
        } else {
            setBoxDataInvalid();
        }

        for (auto& callback : errorCallbacks) {
            if (callback) {
                callback(type, error);
            }
        }
    }

    bool isInitialized() const {
        return initialized;
    }

    uint32_t getUpdateCallCount() const {
        return updateCallCount;
    }

    size_t getHeaterTempCallbackCount() const {
        return heaterTempCallbacks.size();
    }

    size_t getBoxDataCallbackCount() const {
        return boxDataCallbacks.size();
    }

    size_t getErrorCallbackCount() const {
        return errorCallbacks.size();
    }
};

#endif