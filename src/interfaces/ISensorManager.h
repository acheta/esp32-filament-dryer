#ifndef I_SENSOR_MANAGER_H
#define I_SENSOR_MANAGER_H

#include "../Types.h"

/**
 * Interface for SensorManager
 *
 * Responsibilities:
 * - Coordinate reading from multiple sensors at different rates
 * - Maintain cached readings with timestamps and validity
 * - Push interface: Fire callbacks on new readings
 * - Pull interface: Provide cached values on demand
 * - Detect and report sensor failures
 *
 * Does NOT:
 * - Process or interpret readings
 * - Enforce temperature limits
 * - Make control decisions
 */
class ISensorManager {
public:
    virtual ~ISensorManager() = default;

    // Lifecycle
    virtual void begin() = 0;
    virtual void update(uint32_t currentMillis) = 0;

    // Callback registration (push interface)
    virtual void registerHeaterTempCallback(HeaterTempCallback callback) = 0;
    virtual void registerBoxDataCallback(BoxDataCallback callback) = 0;
    virtual void registerSensorErrorCallback(SensorErrorCallback callback) = 0;

    // Getters (pull interface)
    virtual SensorReadings getReadings() const = 0;
    virtual float getHeaterTemp() const = 0;
    virtual float getBoxTemp() const = 0;
    virtual float getBoxHumidity() const = 0;
    virtual bool isHeaterTempValid() const = 0;
    virtual bool isBoxDataValid() const = 0;
};

#endif