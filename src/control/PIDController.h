#ifndef PID_CONTROLLER_H
#define PID_CONTROLLER_H

#include "../interfaces/IPIDController.h"
#include "../Config.h"

/**
 * PIDController - PID algorithm with anti-windup and temperature-aware slowdown
 *
 * Features:
 * - Three tuning profiles (SOFT, NORMAL, STRONG)
 * - Anti-windup protection
 * - Derivative smoothing via low-pass filter
 * - Temperature-aware slowdown (primary overshoot prevention)
 * - Derivative on measurement (not error) to avoid setpoint kick
 */
class PIDController : public IPIDController {
private:
    // Tuning parameters
    float kp, ki, kd;

    // Output limits
    float outMin, outMax;

    // Temperature limit
    float maxAllowedTemp;

    // State
    float integral;
    float lastInput;
    float filteredDerivative;
    uint32_t lastTime;
    bool firstRun;

    // Constants
    static constexpr float DERIVATIVE_FILTER_ALPHA = PID_DERIVATIVE_FILTER_ALPHA;
    static constexpr float TEMP_SLOWDOWN_MARGIN = PID_TEMP_SLOWDOWN_MARGIN;

    void setTuning(float p, float i, float d) {
        kp = p;
        ki = i;
        kd = d;
    }

public:
    PIDController()
        : kp(PID_NORMAL.kp),
          ki(PID_NORMAL.ki),
          kd(PID_NORMAL.kd),
          outMin(PWM_MIN),
          outMax(PWM_MAX),
          maxAllowedTemp(MAX_HEATER_TEMP),
          integral(0.0),
          lastInput(0.0),
          filteredDerivative(0.0),
          lastTime(0),
          firstRun(true) {
    }

    void begin() override {
        reset();
    }

    void setProfile(PIDProfile profile) override {
        switch (profile) {
            case PIDProfile::SOFT:
                setTuning(PID_SOFT.kp, PID_SOFT.ki, PID_SOFT.kd);
                break;
            case PIDProfile::NORMAL:
                setTuning(PID_NORMAL.kp, PID_NORMAL.ki, PID_NORMAL.kd);
                break;
            case PIDProfile::STRONG:
                setTuning(PID_STRONG.kp, PID_STRONG.ki, PID_STRONG.kd);
                break;
        }
    }

    void setLimits(float outMinVal, float outMaxVal) override {
        outMin = outMinVal;
        outMax = outMaxVal;
    }

    void setMaxAllowedTemp(float maxTemp) override {
        maxAllowedTemp = maxTemp;
    }

    float compute(float setpoint, float input, uint32_t currentMillis) override {
        // First run initialization
        if (firstRun) {
            lastInput = input;
            lastTime = currentMillis;
            firstRun = false;
            return 0.0;
        }

        // Calculate time delta
        float dt = currentMillis - lastTime;
        if (dt <= 0) {
            return constrain(integral, outMin, outMax); // No time passed, return last output
        }
        float dtSec = dt / 1000.0;

        // Calculate error
        float error = setpoint - input;

        // Proportional term
        float pTerm = kp * error;

        // Integral term with anti-windup
        float proposedIntegral = integral + ki * error * dtSec;

        // Calculate output with proposed integral
        float proposedOutput = pTerm + proposedIntegral;

        // Anti-windup: don't accumulate integral if output would saturate
        if (proposedOutput > outMax && error > 0) {
            // Output maxed and error still positive - don't accumulate more
            proposedIntegral = integral;
        } else if (proposedOutput < outMin && error < 0) {
            // Output at min and error still negative - don't accumulate more
            proposedIntegral = integral;
        }

        integral = proposedIntegral;
        integral = constrain(integral, outMin, outMax); // Clamp to output range

        // Derivative term with low-pass filter (on measurement, not error)
        float dInput = (input - lastInput) / dtSec;
        float rawDerivative = -kd * dInput; // Negative to oppose change

        // Apply exponential moving average filter
        filteredDerivative = DERIVATIVE_FILTER_ALPHA * rawDerivative
                           + (1.0 - DERIVATIVE_FILTER_ALPHA) * filteredDerivative;

        float dTerm = filteredDerivative;

        // Calculate output
        float output = pTerm + integral + dTerm;
        output = constrain(output, outMin, outMax);

        // Temperature-aware slowdown (PRIMARY overshoot prevention)
        float tempMargin = maxAllowedTemp - input;

        if (input >= maxAllowedTemp) {
            // Emergency: at or above max temp, stop completely
            output = 0.0;
            integral = 0.0; // Clear integral to prevent windup
        } else if (tempMargin < TEMP_SLOWDOWN_MARGIN && tempMargin > 0) {
            // Approaching max temp: scale output proportionally
            float scaleFactor = tempMargin / TEMP_SLOWDOWN_MARGIN; // 0-1 range
            output *= scaleFactor;

            // Also scale down integral to prevent windup when limiting
            integral *= scaleFactor;
        }

        // Update state
        lastInput = input;
        lastTime = currentMillis;

        return output;
    }

    void reset() override {
        integral = 0.0;
        filteredDerivative = 0.0;
        lastInput = 0.0;
        firstRun = true;
    }
};

#endif