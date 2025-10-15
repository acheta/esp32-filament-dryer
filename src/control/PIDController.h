#ifndef PID_CONTROLLER_H
#define PID_CONTROLLER_H

#include "../interfaces/IPIDController.h"
#include "../Config.h"

/**
 * PIDController - PID algorithm with box temperature control and heater limiting
 *
 * Features:
 * - Three tuning profiles (SOFT, NORMAL, STRONG)
 * - Anti-windup protection
 * - Derivative smoothing via low-pass filter
 * - Predictive cooling compensation (prevents undershoot during cooldown)
 * - Derivative on measurement (not error) to avoid setpoint kick
 * - Output limited to PWM_MAX_PID_OUTPUT to prevent overpowering
 * - Two-phase heater limiting based on box temperature proximity to target:
 *   * Aggressive phase: Box far from target → Allow heater up to maxAllowedTemp
 *   * Conservative phase: Box near target → Reduce heater limit to prevent overshoot
 *
 * IMPORTANT: PID controls BOX temperature, not heater temperature
 *            maxAllowedTemp is the maximum heater temperature (e.g., targetBox + overshoot)
 *            Example: For 50°C box target with 10°C overshoot → maxAllowedTemp = 60°C
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
    float coolingRate;  // Track cooling rate separately
    uint32_t lastTime;
    bool firstRun;

    // Constants
    static constexpr float DERIVATIVE_FILTER_ALPHA = PID_DERIVATIVE_FILTER_ALPHA;
    static constexpr float TEMP_SLOWDOWN_MARGIN = PID_TEMP_SLOWDOWN_MARGIN;

    // Predictive cooling parameters
    static constexpr float COOLING_RATE_FILTER_ALPHA = 0.7;  // Filter for cooling rate
    static constexpr float PREDICTIVE_HORIZON_SEC = 10.0;    // Look ahead 10 seconds
    static constexpr float MIN_COOLING_RATE = -0.08;          // °C/s - only predict if cooling faster
    static constexpr float PREDICTIVE_GAIN = 1.5;            // Amplify response during cooling

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
          outMax(PWM_MAX_PID_OUTPUT),  // Use PWM_MAX_PID_OUTPUT instead of PWM_MAX
          maxAllowedTemp(MAX_HEATER_TEMP),
          integral(0.0),
          lastInput(0.0),
          filteredDerivative(0.0),
          coolingRate(0.0),
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
        // Cap the max value to PWM_MAX_PID_OUTPUT
        outMax = (outMaxVal > PWM_MAX_PID_OUTPUT) ? PWM_MAX_PID_OUTPUT : outMaxVal;
    }

    void setMaxAllowedTemp(float maxTemp) override {
        maxAllowedTemp = maxTemp;
    }

    float compute(float setpoint, float boxTemp, float heaterTemp, uint32_t currentMillis) override {
        // First run initialization
        if (firstRun) {
            lastInput = boxTemp;
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

        // Calculate current box temperature rate of change
        float rawCoolingRate = (boxTemp - lastInput) / dtSec;  // °C/s (negative when cooling)

        // Apply exponential moving average to cooling rate
        coolingRate = COOLING_RATE_FILTER_ALPHA * rawCoolingRate
                    + (1.0 - COOLING_RATE_FILTER_ALPHA) * coolingRate;

        // Calculate error based on BOX temperature (primary control variable)
        float error = setpoint - boxTemp;

        // Predictive compensation for cooling
        // If we're cooling down (negative rate) and above setpoint,
        // predict where we'll be in PREDICTIVE_HORIZON_SEC
        float predictedError = error;

        if (coolingRate < MIN_COOLING_RATE) {  // Cooling faster than threshold
            // Predict future temperature
            float predictedTemp = boxTemp + (coolingRate * PREDICTIVE_HORIZON_SEC);
            predictedError = setpoint - predictedTemp;

            // If prediction shows we'll undershoot, increase error now
            if (predictedError > error) {
                // Blend predicted error with current error
                error = error + (predictedError - error) * PREDICTIVE_GAIN;

                #ifdef DEBUG_PID
                Serial.print("COOLING PREDICTION: Rate=");
                Serial.print(coolingRate, 3);
                Serial.print(" °C/s | Predicted temp=");
                Serial.print(predictedTemp, 1);
                Serial.print(" | Enhanced error=");
                Serial.println(error, 2);
                #endif
            }
        }

        // Proportional term (using potentially enhanced error)
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
        float dInput = (boxTemp - lastInput) / dtSec;
        float rawDerivative = -kd * dInput; // Negative to oppose change

        // Apply exponential moving average filter
        filteredDerivative = DERIVATIVE_FILTER_ALPHA * rawDerivative
                           + (1.0 - DERIVATIVE_FILTER_ALPHA) * filteredDerivative;

        float dTerm = filteredDerivative;

        // Calculate output
        float output = pTerm + integral + dTerm;
        output = constrain(output, outMin, outMax);

        // ==================== Two-Phase Heater Limiting ====================
        // Phase 1: Box far from target → Allow heater up to maxAllowedTemp (aggressive)
        // Phase 2: Box near target → Reduce heater limit to prevent box overshoot (conservative)

        float boxError = setpoint - boxTemp;  // Positive when box is below target

        // Calculate dynamic heater limit based on box temperature proximity
        float dynamicHeaterLimit = maxAllowedTemp;

        if (boxError <= BOX_TEMP_APPROACH_MARGIN && boxError > 0) {
            // Box is approaching target - transition to conservative mode
            // Reduce heater limit proportionally as box gets closer
            // When box is within BOX_TEMP_APPROACH_MARGIN of target, start scaling down heater limit

            float approachRatio = boxError / BOX_TEMP_APPROACH_MARGIN; // 1.0 (far) to 0.0 (at target)

            // At target: heater limit = setpoint + MAX_BOX_TEMP_OVERSHOOT
            // Far from target: heater limit = maxAllowedTemp (full overshoot)
            float minHeaterLimit = setpoint + MAX_BOX_TEMP_OVERSHOOT;
            dynamicHeaterLimit = minHeaterLimit + (maxAllowedTemp - minHeaterLimit) * approachRatio;

            #ifdef DEBUG_PID
            Serial.print("CONSERVATIVE MODE: BoxError=");
            Serial.print(boxError, 2);
            Serial.print("°C | ApproachRatio=");
            Serial.print(approachRatio, 2);
            Serial.print(" | HeaterLimit=");
            Serial.println(dynamicHeaterLimit, 1);
            #endif
        } else if (boxError <= 0) {
            // Box is at or above target - minimum heater limit
            dynamicHeaterLimit = setpoint + MAX_BOX_TEMP_OVERSHOOT;

            #ifdef DEBUG_PID
            Serial.print("BOX AT TARGET: HeaterLimit=");
            Serial.println(dynamicHeaterLimit, 1);
            #endif
        }

        // Apply heater temperature limiting based on dynamic limit
        float heaterMargin = dynamicHeaterLimit - heaterTemp;

        if (heaterTemp >= dynamicHeaterLimit) {
            // Heater at or above dynamic limit - stop heating
            output = 0.0;
            integral = 0.0; // Clear integral to prevent windup

            #ifdef DEBUG_PID
            Serial.print("HEATER LIMIT REACHED: Heater=");
            Serial.print(heaterTemp, 1);
            Serial.print("°C | Limit=");
            Serial.println(dynamicHeaterLimit, 1);
            #endif
        } else if (heaterMargin < TEMP_SLOWDOWN_MARGIN && heaterMargin > 0) {
            // Approaching heater limit: scale output proportionally
            float scaleFactor = heaterMargin / TEMP_SLOWDOWN_MARGIN; // 0-1 range
            output *= scaleFactor;

            // Also scale down integral to prevent windup when limiting
            integral *= scaleFactor;

            #ifdef DEBUG_PID
            Serial.print("HEATER SLOWDOWN: Margin=");
            Serial.print(heaterMargin, 2);
            Serial.print("°C | Scale=");
            Serial.print(scaleFactor, 2);
            Serial.print(" | Output reduced to ");
            Serial.println(output, 1);
            #endif
        }

        // Update state (store box temp for derivative calculation)
        lastInput = boxTemp;
        lastTime = currentMillis;

        #ifdef DEBUG_PID
        Serial.print("PID: BoxSP=");
        Serial.print(setpoint, 1);
        Serial.print(" | BoxTemp=");
        Serial.print(boxTemp, 1);
        Serial.print(" | HeaterTemp=");
        Serial.print(heaterTemp, 1);
        Serial.print(" | ERR=");
        Serial.print(error, 2);
        Serial.print(" | P=");
        Serial.print(pTerm, 1);
        Serial.print(" | I=");
        Serial.print(integral, 1);
        Serial.print(" | D=");
        Serial.print(dTerm, 1);
        Serial.print(" | OUT=");
        Serial.println(output, 1);
        #endif

        return output;
    }

    void reset() override {
        integral = 0.0;
        filteredDerivative = 0.0;
        coolingRate = 0.0;
        lastInput = 0.0;
        firstRun = true;
    }

    // Debug getter
    float getCoolingRate() const {
        return coolingRate;
    }

    // Debug getter for actual output limits
    float getOutputMax() const {
        return outMax;
    }
};

#endif