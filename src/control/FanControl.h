#ifndef FAN_CONTROL_H
#define FAN_CONTROL_H

#include "../interfaces/IFanControl.h"
#include "../Config.h"

/**
 * FanControl - Simple relay control for cooling fan
 *
 * Controls a relay connected to a fan. The fan should run whenever
 * the dryer is actively heating or paused to ensure proper air circulation.
 */
class FanControl : public IFanControl {
private:
    uint8_t pin;
    bool running;

public:
    /**
     * Constructor - initializes pin and ensures fan is off
     *
     * @param fanPin - GPIO pin connected to fan relay
     */
    FanControl(uint8_t fanPin)
        : pin(fanPin), running(false) {
#ifndef UNIT_TEST
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);  // Ensure fan is off initially
#endif
    }

    void start() override {
        running = true;
#ifndef UNIT_TEST
        digitalWrite(pin, HIGH);
#endif
    }

    void stop() override {
        running = false;
#ifndef UNIT_TEST
        digitalWrite(pin, LOW);
#endif
    }

    bool isRunning() const override {
        return running;
    }
};

#endif