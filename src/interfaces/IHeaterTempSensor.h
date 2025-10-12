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
 * - Read temperature from heater sensor (async and sync modes)
 * - Validate readings
 * - Report sensor status
 *
 * Async Pattern:
 * - Call requestConversion() to start temperature measurement
 * - Wait ~750ms for 12-bit conversion
 * - Call isConversionReady() to check if ready
 * - Call read() to retrieve the result
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

    // Synchronous read (backward compatible, may block up to 750ms)
    virtual bool read() = 0;

    // Asynchronous read pattern (non-blocking)
    virtual void requestConversion() = 0;
    virtual bool isConversionReady() = 0;

    virtual float getTemperature() const = 0;
    virtual bool isValid() const = 0;
    virtual String getLastError() const = 0;
};

#endif