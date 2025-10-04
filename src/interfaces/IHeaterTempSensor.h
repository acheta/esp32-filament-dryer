#ifndef I_HEATER_TEMP_SENSOR_H
#define I_HEATER_TEMP_SENSOR_H

#ifndef UNIT_TEST
    #include <Arduino.h>
#else
    // Mock Arduino functions for native testing
    #include "../../test/mocks/arduino_mock.h"
#endif

/**
 * Interface for Heater Temperature Sensor (DS18B20)
 *
 * Responsibilities:
 * - Read temperature from heater sensor
 * - Validate readings
 * - Report sensor status
 *
 * Does NOT:
 * - Manage update timing (handled by SensorManager)
 * - Fire callbacks (handled by SensorManager)
 * - Enforce temperature limits
 */
class IHeaterTempSensor {
public:
    virtual ~IHeaterTempSensor() = default;

    virtual void begin() = 0;
    virtual bool read() = 0;
    virtual float getTemperature() const = 0;
    virtual bool isValid() const = 0;
    virtual String getLastError() const = 0;
};

#endif