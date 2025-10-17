#ifndef PID_CONTROLLER_H
#define PID_CONTROLLER_H
#define DEBUG_PID
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

    // Steady-state tracking
    float steadyStateOutput;        // Learned output that maintains target temperature
    uint32_t steadyStateStartTime;  // When we entered steady-state
    bool inSteadyState;             // Are we currently in steady-state?

    // Heater-Box correlation tracking
    float heaterRate;               // Track heater temperature rate of change
    float lastHeaterTemp;           // Last heater temperature reading

    // Baseline insufficiency tracking
    bool baselineEnforced;          // Is minimum baseline currently being enforced?
    uint32_t baselineEnforcementStartTime;  // When baseline enforcement began

    // Constants
    static constexpr float DERIVATIVE_FILTER_ALPHA = PID_DERIVATIVE_FILTER_ALPHA;
    static constexpr float TEMP_SLOWDOWN_MARGIN = PID_TEMP_SLOWDOWN_MARGIN;

    // Predictive cooling parameters (REDUCED aggressiveness)
    static constexpr float COOLING_RATE_FILTER_ALPHA = 0.95;  // Filter for cooling rate
    static constexpr float PREDICTIVE_HORIZON_SEC = 15.0;     // Look ahead 15 seconds (reduced from 30)
    static constexpr float MIN_COOLING_RATE = -0.2;          // °C/s - only predict if cooling much faster (was -0.03)
    static constexpr float PREDICTIVE_GAIN = 1;             // Amplify response during cooling (reduced from 4)

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
          firstRun(true),
          steadyStateOutput(0.0),
          steadyStateStartTime(0),
          inSteadyState(false),
          heaterRate(0.0),
          lastHeaterTemp(0.0),
          baselineEnforced(false),
          baselineEnforcementStartTime(0) {
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
            lastHeaterTemp = heaterTemp;
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

        // ==================== Heater Rate Tracking (Leading Indicator) ====================
        // Track heater temperature rate of change - heater leads box by ~20 seconds
        float rawHeaterRate = (heaterTemp - lastHeaterTemp) / dtSec;  // °C/s
        heaterRate = HEATER_BOX_CORRELATION_FILTER * rawHeaterRate
                   + (1.0 - HEATER_BOX_CORRELATION_FILTER) * heaterRate;

        // Calculate current box temperature rate of change
        float rawCoolingRate = (boxTemp - lastInput) / dtSec;  // °C/s (negative when cooling)

        // Apply exponential moving average to cooling rate
        coolingRate = COOLING_RATE_FILTER_ALPHA * rawCoolingRate
                    + (1.0 - COOLING_RATE_FILTER_ALPHA) * coolingRate;

        // Calculate error based on BOX temperature (primary control variable)
        float error = setpoint - boxTemp;

        // ==================== Predictive Compensation (REDUCED Aggressiveness) ====================
        // Predict cooling and undershoot - now with much stricter activation threshold
        // Only activate when cooling is RAPID (< -0.15°C/s, was -0.03°C/s)

        bool coolingPredictionActive = false;

        if (coolingRate < MIN_COOLING_RATE && boxTemp > setpoint - 1.0) {
            // Cooling rapidly AND still above/near target
            float predictedTemp = boxTemp + (coolingRate * PREDICTIVE_HORIZON_SEC);
            float predictedError = setpoint - predictedTemp;

            // If prediction shows significant undershoot, increase error
            if (predictedError > error && predictedError > 1.0) {
                // Blend predicted error with current error (reduced gain: 1.5 was 4.0)
                error = error + (predictedError - error) * PREDICTIVE_GAIN;
                coolingPredictionActive = true;

                #ifdef DEBUG_PID
                Serial.print("COOLING PREDICTION: BoxRate=");
                Serial.print(coolingRate, 3);
                Serial.print(" HeaterRate=");
                Serial.print(heaterRate, 3);
                Serial.print(" °C/s | Predicted=");
                Serial.print(predictedTemp, 1);
                Serial.print("°C | Enhanced error=");
                Serial.println(error, 2);
                #endif
            }
        }

        // Proportional term (using potentially enhanced error)
        float pTerm = kp * error;

        // ==================== Integral Term with Smooth Windup Protection ====================
        float proposedIntegral = integral + ki * error * dtSec;

        // Calculate output with proposed integral
        float proposedOutput = pTerm + proposedIntegral;

        // Smooth anti-windup: gradually reduce integral accumulation near saturation
        if (proposedOutput > outMax && error > 0) {
            // Output approaching max - don't accumulate more, but decay smoothly
            proposedIntegral = integral * 0.95;  // Decay instead of hard zeroing
        } else if (proposedOutput < outMin && error < 0) {
            // Output approaching min - decay smoothly
            proposedIntegral = integral * 0.95;
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

        // ==================== Coordinated Heater Limiting ====================
        // Apply heater temperature limiting, but coordinate with cooling prediction
        float heaterMargin = dynamicHeaterLimit - heaterTemp;

        if (heaterTemp >= dynamicHeaterLimit) {
            // Heater at or above dynamic limit - stop heating
            output = 0.0;
            integral *= 0.5; // Smooth decay instead of hard zero

            #ifdef DEBUG_PID
            Serial.print("HEATER LIMIT REACHED: Heater=");
            Serial.print(heaterTemp, 1);
            Serial.print("°C | Limit=");
            Serial.println(dynamicHeaterLimit, 1);
            #endif
        } else if (heaterMargin < TEMP_SLOWDOWN_MARGIN && heaterMargin > 0) {
            // Approaching heater limit: scale output proportionally
            float scaleFactor = heaterMargin / TEMP_SLOWDOWN_MARGIN; // 0-1 range

            // If cooling prediction is active, reduce slowdown aggressiveness
            // (let prediction win to prevent oscillation)
            if (coolingPredictionActive) {
                scaleFactor = scaleFactor * 0.7 + 0.3; // Soften scaling (min 0.3 instead of 0.0)
            }

            output *= scaleFactor;

            // Smooth integral decay proportional to scaling
            integral *= (scaleFactor * 0.5 + 0.5);  // Decay less aggressively

            #ifdef DEBUG_PID
            Serial.print("HEATER SLOWDOWN: Margin=");
            Serial.print(heaterMargin, 2);
            Serial.print("°C | Scale=");
            Serial.print(scaleFactor, 2);
            Serial.print(" | Output reduced to ");
            Serial.println(output, 1);
            #endif
        }

        // ==================== Minimum Heater Temperature Control ====================
        // When box is at or near target, ensure heater doesn't drop below setpoint
        // This provides steady-state stability and prevents oscillations

        if (boxError <= 0.5) {  // Box is at or very close to target
            float minHeaterTemp = setpoint - MIN_HEATER_TEMP_MARGIN;  // Allow small deadband

            if (heaterTemp < minHeaterTemp && output < outMax) {
                // Heater dropped below minimum - ensure some output to bring it back up
                // Calculate minimum output needed (simple proportional boost)
                float heaterDeficit = minHeaterTemp - heaterTemp;
                float minOutput = heaterDeficit * 2.0;  // Proportional gain for minimum control

                // Ensure output is at least minOutput (but respect max limits)
                if (output < minOutput) {
                    output = constrain(minOutput, output, outMax);

                    #ifdef DEBUG_PID
                    Serial.print("MIN HEATER CONTROL: Heater=");
                    Serial.print(heaterTemp, 1);
                    Serial.print("°C | MinLimit=");
                    Serial.print(minHeaterTemp, 1);
                    Serial.print(" | Output boosted to ");
                    Serial.println(output, 1);
                    #endif
                }
            }
        }

        // Calculate absolute box error for use in multiple sections below
        float absBoxError = (boxError >= 0) ? boxError : -boxError;

        // ==================== Heater Momentum Compensation ====================
        // Heater has significant thermal mass + sensor lag
        // When heater cools rapidly, apply proactive heating to prevent undershoot

        if (heaterRate < HEATER_MOMENTUM_THRESHOLD && absBoxError < 2.0) {
            // Heater cooling rapidly while near target - add compensating output
            float momentumCompensation = -heaterRate * HEATER_MOMENTUM_GAIN;  // Negative rate * gain = positive boost
            output += momentumCompensation;
            output = constrain(output, outMin, outMax);

            #ifdef DEBUG_PID
            Serial.print("HEATER MOMENTUM COMP: HeaterRate=");
            Serial.print(heaterRate, 3);
            Serial.print("°C/s | Boost=+");
            Serial.print(momentumCompensation, 1);
            Serial.println("%");
            #endif
        }

        // ==================== Minimum Output Near Target ====================
        // CRITICAL: Prevent excessive cooling undershoot
        // When near target, never drop below minimum baseline output
        // Empirical data shows ~20% maintains ~50°C heater temp

        bool wasBaselineEnforced = false;
        if (absBoxError < 2.0 && output < MIN_OUTPUT_NEAR_TARGET) {
            // Near target but output too low - apply minimum baseline
            float originalOutput = output;
            output = MIN_OUTPUT_NEAR_TARGET;
            wasBaselineEnforced = true;

            #ifdef DEBUG_PID
            Serial.print("MIN OUTPUT ENFORCED: Was=");
            Serial.print(originalOutput, 1);
            Serial.print("% | Now=");
            Serial.print(output, 1);
            Serial.println("%");
            #endif
        }

        // ==================== Baseline Insufficiency Compensation ====================
        // When baseline is enforced but box temperature is still falling,
        // proactively boost output above baseline to prevent undershoot

        if (wasBaselineEnforced) {
            // Track baseline enforcement duration
            if (!baselineEnforced) {
                // Just started enforcing baseline
                baselineEnforced = true;
                baselineEnforcementStartTime = currentMillis;
            }

            // Check if baseline has been enforced long enough to assess insufficiency
            uint32_t baselineEnforcementDuration = currentMillis - baselineEnforcementStartTime;

            if (baselineEnforcementDuration >= BASELINE_ENFORCEMENT_THRESHOLD_MS) {
                // Baseline has been active long enough - check if box is cooling
                if (coolingRate < 0) {
                    // Box is cooling despite baseline - boost output proportionally
                    float baselineBoost = -coolingRate * BASELINE_BOOST_GAIN;  // Negative rate * gain = positive boost
                    baselineBoost = constrain(baselineBoost, 0.0f, MAX_BASELINE_BOOST);

                    output += baselineBoost;
                    output = constrain(output, outMin, outMax);

                    #ifdef DEBUG_PID
                    Serial.print("BASELINE INSUFFICIENT: BoxRate=");
                    Serial.print(coolingRate, 3);
                    Serial.print("°C/s | Duration=");
                    Serial.print(baselineEnforcementDuration / 1000.0, 1);
                    Serial.print("s | Boost=+");
                    Serial.print(baselineBoost, 1);
                    Serial.print("% | Output=");
                    Serial.println(output, 1);
                    #endif
                }
            }
        } else {
            // Baseline not enforced - reset tracking
            if (baselineEnforced) {
                baselineEnforced = false;
                baselineEnforcementStartTime = 0;
            }
        }

        // ==================== Steady-State Learning ====================
        // Track output when system is stable at target for extended period
        // This learned value helps stabilize future approaches to target

        if (absBoxError <= STEADY_STATE_TOLERANCE) {
            // Box is within tolerance of target
            if (!inSteadyState) {
                // Just entered steady-state
                steadyStateStartTime = currentMillis;
                inSteadyState = true;
            } else {
                // Still in steady-state - check if we've been here long enough to trust the output
                uint32_t timeInSteadyState = currentMillis - steadyStateStartTime;

                if (timeInSteadyState >= STEADY_STATE_TIME_MS) {
                    // We've been stable for sufficient time - learn this output
                    // Use exponential filter to smooth learned value
                    if (steadyStateOutput == 0.0) {
                        // First time learning
                        steadyStateOutput = output;
                    } else {
                        // Update learned value with filtering
                        steadyStateOutput = STEADY_STATE_OUTPUT_FILTER * steadyStateOutput
                                          + (1.0 - STEADY_STATE_OUTPUT_FILTER) * output;
                    }

                    #ifdef DEBUG_PID
                    if (timeInSteadyState % 10000 < dt) {  // Log every ~10 seconds
                        Serial.print("STEADY-STATE LEARNING: Output=");
                        Serial.print(output, 1);
                        Serial.print("% | Learned=");
                        Serial.print(steadyStateOutput, 1);
                        Serial.println("%");
                    }
                    #endif
                }
            }

            // Bias output toward learned steady-state value
            // This helps prevent oscillations and speeds convergence
            if (steadyStateOutput > 0.0) {
                if (absBoxError < 0.5) {
                    // Very close to target - strongly bias toward learned output
                    float biasFactor = 0.4;  // Blend 40% toward learned value

                    // If box is below target (cooling undershoot), bias even more strongly
                    if (boxError > 0) {  // Box below target
                        biasFactor = 0.6;  // 60% bias to prevent undershoot
                    }

                    output = output * (1.0 - biasFactor) + steadyStateOutput * biasFactor;

                    #ifdef DEBUG_PID
                    Serial.print("STEADY-STATE BIAS: Factor=");
                    Serial.print(biasFactor, 2);
                    Serial.print(" | Output adjusted to ");
                    Serial.println(output, 1);
                    #endif
                }
            }
        } else {
            // Left steady-state zone
            inSteadyState = false;
        }

        // Update state (store temps for rate calculation)
        lastInput = boxTemp;
        lastHeaterTemp = heaterTemp;
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
        Serial.print(output, 1);
        if (steadyStateOutput > 0.0) {
            Serial.print(" | SS=");
            Serial.print(steadyStateOutput, 1);
        }
        Serial.println();
        #endif

        return output;
    }

    void reset() override {
        integral = 0.0;
        filteredDerivative = 0.0;
        coolingRate = 0.0;
        lastInput = 0.0;
        firstRun = true;
        steadyStateOutput = STEADY_STATE_MIN_OUTPUT;  // Initialize to baseline (~20%)
        steadyStateStartTime = 0;
        inSteadyState = false;
        heaterRate = 0.0;
        lastHeaterTemp = 0.0;
        baselineEnforced = false;
        baselineEnforcementStartTime = 0;
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