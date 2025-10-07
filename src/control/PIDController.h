#ifndef PID_CONTROLLER_H
#define PID_CONTROLLER_H

#include "../interfaces/IPIDController.h"
#include "../Config.h"

/**
 * PIDController - PID algorithm optimized for high thermal inertia
 *
 * Features:
 * - Three tuning profiles (SOFT, NORMAL, STRONG)
 * - Anti-windup protection
 * - Derivative smoothing via low-pass filter
 * - Exponential temperature-aware slowdown
 * - Predictive overshoot prevention based on rise rate
 * - Enhanced derivative term for momentum control
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
    float lastLastInput;  // Two samples back for rate calculation
    float filteredDerivative;
    uint32_t lastTime;
    uint32_t lastLastTime;
    bool firstRun;

    // Constants
    static constexpr float DERIVATIVE_FILTER_ALPHA = 0.7;  // Low-pass filter, TODO: from config
    static constexpr float TEMP_SLOWDOWN_MARGIN = 15.0;    // Start scaling 15°C early, TODO: from config
    static constexpr float PREDICTION_HORIZON = 15.0;       // Predict 20 seconds ahead TODO: from config

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
          lastLastInput(0.0),
          filteredDerivative(0.0),
          lastTime(0),
          lastLastTime(0),
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
            lastLastInput = input;
            lastTime = currentMillis;
            lastLastTime = currentMillis;
            firstRun = false;
            return 0.0;
        }

        // Calculate time delta
        float dt = currentMillis - lastTime;
        if (dt <= 0) {
            return constrain(integral, outMin, outMax);
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
            proposedIntegral = integral;
        } else if (proposedOutput < outMin && error < 0) {
            proposedIntegral = integral;
        }

        integral = proposedIntegral;
        integral = constrain(integral, outMin, outMax);

        // ===== ENHANCED DERIVATIVE TERM =====
        // Derivative on measurement (not error) to avoid setpoint kick
        float dInput = (input - lastInput) / dtSec;
        float rawDerivative = -kd * dInput;

        // Boost derivative when rapidly approaching setpoint
        // This fights thermal momentum
        if (dInput > 0.3 && error > 0 && error < 15.0) {
            // Rising fast while close to target: apply 2x derivative braking
            rawDerivative *= 2.0;
        }

        // Apply exponential moving average filter
        filteredDerivative = DERIVATIVE_FILTER_ALPHA * rawDerivative
                           + (1.0 - DERIVATIVE_FILTER_ALPHA) * filteredDerivative;

        float dTerm = filteredDerivative;

        // Calculate base output
        float output = pTerm + integral + dTerm;
        output = constrain(output, outMin, outMax);

        // ===== PREDICTIVE OVERSHOOT PREVENTION =====
        // Calculate temperature rise rate and predict future temperature
        if (lastLastTime > 0 && (currentMillis - lastLastTime) > 0) {
            float dtHistory = (currentMillis - lastLastTime) / 1000.0;
            float riseRate = (input - lastLastInput) / dtHistory;  // °C/second

            // Predict temperature after PREDICTION_HORIZON seconds
            float predictedTemp = input + (riseRate * PREDICTION_HORIZON);

            // If we'll overshoot, reduce power NOW
            if (predictedTemp > setpoint) {
                float predictedOvershoot = predictedTemp - setpoint;

                if (predictedOvershoot > 15.0) {
                    // Severe predicted overshoot - emergency brake
                    output *= 0.05;  // Cut to 5%
                    integral *= 0.2;
                } else if (predictedOvershoot > 10.0) {
                    // Large predicted overshoot
                    output *= 0.15;  // Cut to 15%
                    integral *= 0.4;
                } else if (predictedOvershoot > 5.0) {
                    // Moderate predicted overshoot
                    output *= 0.35;  // Cut to 35%
                    integral *= 0.6;
                } else if (predictedOvershoot > 2.0) {
                    // Mild predicted overshoot
                    output *= 0.65;  // Cut to 65%
                    integral *= 0.8;
                }
            } else if (predictedTemp < setpoint - 5.0) {
                // We're well below target and cooling down - can afford to be more aggressive
                output = min((float)(output * 1.2), outMax); // Boost by 20%
            }
        }

        // ===== EXPONENTIAL TEMPERATURE-AWARE SLOWDOWN =====
        // More aggressive than linear scaling
        float tempMargin = maxAllowedTemp - input;

        if (input >= maxAllowedTemp) {
            // Emergency: at or above max temp, stop completely
            output = 0.0;
            integral = 0.0;
        } else if (tempMargin < TEMP_SLOWDOWN_MARGIN && tempMargin > 0) {
            // Approaching max temp: use exponential scaling
            float linearScale = tempMargin / TEMP_SLOWDOWN_MARGIN;

            // Square the scale factor for exponential reduction
            // At 5°C margin: scale = (5/15)² ≈ 0.11 (11% power)
            // At 10°C margin: scale = (10/15)² ≈ 0.44 (44% power)
            // At 15°C margin: scale = 1.0 (100% power)
            float scaleFactor = linearScale * linearScale;

            output *= scaleFactor;
            integral *= scaleFactor;
        }

        // Update history for next iteration
        lastLastInput = lastInput;
        lastLastTime = lastTime;
        lastInput = input;
        lastTime = currentMillis;

        return output;
    }

    void reset() override {
        integral = 0.0;
        filteredDerivative = 0.0;
        lastInput = 0.0;
        lastLastInput = 0.0;
        firstRun = true;
    }
};

#endif