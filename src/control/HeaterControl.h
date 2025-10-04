#ifndef HEATER_CONTROL_H
#define HEATER_CONTROL_H

#include "../interfaces/IHeaterControl.h"
#include "../Config.h"

/**
 * HeaterControl - PWM heater control using ESP32 LEDC
 *
 * Simple relay: directly applies PWM value without modification.
 * Trusts PID for proper control and SafetyMonitor for emergencies.
 */
class HeaterControl : public IHeaterControl {
private:
    uint8_t pwmPin;
    uint8_t pwmChannel;
    uint32_t pwmFreq;
    uint8_t pwmResolution;

    bool running;
    uint8_t currentPWM;

public:
    HeaterControl(uint8_t pin = HEATER_PWM_PIN,
                  uint8_t channel = HEATER_PWM_CHANNEL,
                  uint32_t freq = HEATER_PWM_FREQ,
                  uint8_t resolution = PWM_RESOLUTION)
        : pwmPin(pin),
          pwmChannel(channel),
          pwmFreq(freq),
          pwmResolution(resolution),
          running(false),
          currentPWM(0) {
    }

    void begin() override {
#ifndef UNIT_TEST
        ledcSetup(pwmChannel, pwmFreq, pwmResolution);
        ledcAttachPin(pwmPin, pwmChannel);
        ledcWrite(pwmChannel, 0);
#endif
    }

    void start() override {
        running = true;
    }

    void stop() override {
        running = false;
        setPWM(0);
    }

    void emergencyStop() override {
        running = false;
        currentPWM = 0;
#ifndef UNIT_TEST
        ledcWrite(pwmChannel, 0);
#endif
    }

    void setPWM(uint8_t value) override {
        if (!running) {
            value = 0;
        }

        currentPWM = constrain(value, PWM_MIN, PWM_MAX);

#ifndef UNIT_TEST
        ledcWrite(pwmChannel, currentPWM);
#endif
    }

    bool isRunning() const override {
        return running;
    }

    uint8_t getCurrentPWM() const override {
        return currentPWM;
    }
};

#endif