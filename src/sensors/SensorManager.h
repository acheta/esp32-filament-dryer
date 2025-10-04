#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include "../interfaces/ISensorManager.h"
#include "../interfaces/IHeaterTempSensor.h"
#include "../interfaces/IBoxTempHumiditySensor.h"
#include "../Types.h"
#include "../Config.h"
#include <vector>

/**
 * SensorManager - Multi-sensor coordinator
 *
 * Coordinates reading from multiple sensors at different rates,
 * maintains cached readings, and notifies callbacks on updates.
 *
 * Sensors are injected as dependencies for better testability.
 */
class SensorManager : public ISensorManager {
private:
    // Injected sensor dependencies
    IHeaterTempSensor* heaterSensor;
    IBoxTempHumiditySensor* boxSensor;

    // Cached readings
    SensorReading heaterTemp;
    SensorReading boxTemp;
    SensorReading boxHumidity;

    // Timing
    uint32_t lastHeaterUpdate;
    uint32_t lastBoxUpdate;

    // Callbacks
    std::vector<HeaterTempCallback> heaterTempCallbacks;
    std::vector<BoxDataCallback> boxDataCallbacks;
    std::vector<SensorErrorCallback> errorCallbacks;

    void notifyHeaterTemp(float temp, uint32_t timestamp) {
        for (auto& callback : heaterTempCallbacks) {
            if (callback) {
                callback(temp, timestamp);
            }
        }
    }

    void notifyBoxData(float temp, float humidity, uint32_t timestamp) {
        for (auto& callback : boxDataCallbacks) {
            if (callback) {
                callback(temp, humidity, timestamp);
            }
        }
    }

    void notifyError(SensorType type, const String& error) {
        for (auto& callback : errorCallbacks) {
            if (callback) {
                callback(type, error);
            }
        }
    }

    void updateHeaterTemp(uint32_t currentMillis) {
        if (!heaterSensor->read()) {
            // Reading failed
            if (!heaterSensor->isValid()) {
                heaterTemp.isValid = false;
                notifyError(SensorType::HEATER_TEMP, heaterSensor->getLastError());
            }
            return;
        }

        // Successful read
        heaterTemp.value = heaterSensor->getTemperature();
        heaterTemp.timestamp = currentMillis;
        heaterTemp.isValid = true;

        notifyHeaterTemp(heaterTemp.value, currentMillis);
    }

    void updateBoxData(uint32_t currentMillis) {
        if (!boxSensor->read()) {
            // Reading failed
            if (!boxSensor->isValid()) {
                boxTemp.isValid = false;
                boxHumidity.isValid = false;
                notifyError(SensorType::BOX_TEMP, boxSensor->getLastError());
            }
            return;
        }

        // Successful read
        boxTemp.value = boxSensor->getTemperature();
        boxTemp.timestamp = currentMillis;
        boxTemp.isValid = true;

        boxHumidity.value = boxSensor->getHumidity();
        boxHumidity.timestamp = currentMillis;
        boxHumidity.isValid = true;

        notifyBoxData(boxTemp.value, boxHumidity.value, currentMillis);
    }

public:
    /**
     * Constructor with dependency injection
     *
     * @param heater - Heater temperature sensor (DS18B20)
     * @param box - Box temperature/humidity sensor (AM2320)
     */
    SensorManager(IHeaterTempSensor* heater, IBoxTempHumiditySensor* box)
        : heaterSensor(heater),
          boxSensor(box),
          lastHeaterUpdate(0),
          lastBoxUpdate(0) {

        heaterTemp.isValid = false;
        boxTemp.isValid = false;
        boxHumidity.isValid = false;
    }

    void begin() override {
        heaterSensor->begin();
        boxSensor->begin();
    }

    void update(uint32_t currentMillis) override {
        // Update heater temp every 500ms
        if (currentMillis - lastHeaterUpdate >= HEATER_TEMP_INTERVAL) {
            lastHeaterUpdate = currentMillis;
            updateHeaterTemp(currentMillis);
        }

        // Update box data every 2000ms
        if (currentMillis - lastBoxUpdate >= BOX_DATA_INTERVAL) {
            lastBoxUpdate = currentMillis;
            updateBoxData(currentMillis);
        }
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
};

#endif