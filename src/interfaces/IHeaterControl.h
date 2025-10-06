#ifndef I_HEATER_CONTROL_H
#define I_HEATER_CONTROL_H

#ifndef UNIT_TEST
    #include <Arduino.h>
#else
    #include "../../test/mocks/arduino_mock.h"
#endif

class IHeaterControl {
public:
    virtual ~IHeaterControl() = default;
    virtual void begin() = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void emergencyStop() = 0;
    virtual void setPWM(uint8_t value) = 0;
    virtual void update(uint32_t currentMillis) = 0;  // NEW: For software PWM
    virtual bool isRunning() const = 0;
    virtual uint8_t getCurrentPWM() const = 0;
};

#endif