#ifndef I_BOX_TEMP_HUMIDITY_SENSOR_H
#define I_BOX_TEMP_HUMIDITY_SENSOR_H

#ifndef UNIT_TEST
    #include <Arduino.h>
#else
    // Mock Arduino functions for native testing
    #include "../../test/mocks/arduino_mock.h"
#endif

/**
 * Interface for Box Temperature and Humidity Sensor (AM2320)
 *
 * Responsibilities:
 * - Read temperature and humidity from box sensor
 * - Validate readings
 * - Report sensor status
 *
 * Does NOT:
 * - Manage update timing (handled by SensorManager)
 * - Fire callbacks (handled by SensorManager)
 * - Enforce temperature/humidity limits
 */
class IBoxTempHumiditySensor {
public:
    virtual ~IBoxTempHumiditySensor() = default;

    virtual void begin() = 0;
    virtual bool read() = 0;
    virtual float getTemperature() const = 0;
    virtual float getHumidity() const = 0;
    virtual bool isValid() const = 0;
    virtual String getLastError() const = 0;
};

#endif