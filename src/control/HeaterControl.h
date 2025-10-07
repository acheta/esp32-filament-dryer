#ifndef HEATER_CONTROL_H
#define HEATER_CONTROL_H

#include "../interfaces/IHeaterControl.h"
#include "../Config.h"

/**
 * HeaterControl - Software PWM for SSR control with long period
 *
 * Uses software timing instead of LEDC to achieve slow PWM (5s period)
 * suitable for SSR relay longevity.
 */
class HeaterControl : public IHeaterControl {
private:
    uint8_t pwmPin;
    bool running;
    uint8_t currentPWM;

    // Software PWM timing
    uint32_t cycleStartTime;
    uint32_t lastUpdateTime;
    static constexpr uint32_t PWM_PERIOD_MS = 5000;  // 5 second period

    bool pinState;  // Current GPIO state

public:
    HeaterControl(uint8_t pin = HEATER_PWM_PIN)
        : pwmPin(pin),
          running(false),
          currentPWM(0),
          cycleStartTime(0),
          lastUpdateTime(0),
          pinState(false) {
    }

    void begin(uint32_t currentMillis) override {
#ifndef UNIT_TEST
        pinMode(pwmPin, OUTPUT);
        digitalWrite(pwmPin, LOW);
#endif
    }

    void start(uint32_t currentMillis) override {
        running = true;
        cycleStartTime = currentMillis;
        lastUpdateTime =currentMillis;
    }

    void stop(uint32_t currentMillis) override {
        running = false;
        setPWM(0);
#ifndef UNIT_TEST
        digitalWrite(pwmPin, LOW);
#endif
        pinState = false;
    }

    void emergencyStop() override {
        running = false;
        currentPWM = 0;
#ifndef UNIT_TEST
        digitalWrite(pwmPin, LOW);
#endif
        pinState = false;
    }

    void setPWM(uint8_t value) override {
        if (!running) {
            value = 0;
        }



        currentPWM = constrain(value, PWM_MIN, PWM_MAX);

        // Don't update GPIO here - let update() handle timing
    }

    /**
     * Update software PWM state
     * MUST be called frequently (at least every 100ms) from main loop
     */
    void update(uint32_t currentMillis) {

        if (!running) {
            return;
        }

        // Calculate position in PWM cycle
        uint32_t elapsed = currentMillis - cycleStartTime;

        // Reset cycle if period elapsed
        if (elapsed >= PWM_PERIOD_MS) {
            cycleStartTime = currentMillis;
            elapsed = 0;
        }

        // Calculate ON time for this cycle
        uint32_t onTimeMs = (PWM_PERIOD_MS * (uint32_t)currentPWM) / 255;

        // Determine desired pin state
        bool shouldBeHigh = (elapsed < onTimeMs);

        // Only update GPIO if state needs to change
        if (shouldBeHigh != pinState) {
            pinState = shouldBeHigh;
#ifndef UNIT_TEST
            digitalWrite(pwmPin, pinState ? HIGH : LOW);
#endif

            // Debug output for state changes
            #ifdef DEBUG_PWM
            Serial.print("PWM: ");
            Serial.print(pinState ? "ON" : "OFF");
            Serial.print(" | Duty: ");
            Serial.print(currentPWM);
            Serial.print("/255 | Elapsed: ");
            Serial.print(elapsed);
            Serial.print("ms / ");
            Serial.print(PWM_PERIOD_MS);
            Serial.println("ms");
            #endif
        }

        lastUpdateTime = currentMillis;
    }

    bool isRunning() const override {
        return running;
    }

    uint8_t getCurrentPWM() const override {
        return currentPWM;
    }

    // Additional method for debug/monitoring
    bool getPinState() const {
        return pinState;
    }
};

#endif