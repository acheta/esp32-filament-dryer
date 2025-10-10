#ifndef MOCK_PID_CONTROLLER_H
#define MOCK_PID_CONTROLLER_H

#include "../../src/interfaces/IPIDController.h"
#include "../../src/Config.h"

class MockPIDController : public IPIDController {
private:
    bool initialized;
    PIDProfile currentProfile;
    float outputMin;
    float outputMax;
    float maxTemp;
    float fixedOutput;
    uint32_t computeCallCount;
    uint32_t resetCallCount;

    float lastSetpoint;
    float lastInput;
    uint32_t lastTime;

public:
    MockPIDController()
        : initialized(false),
          currentProfile(PIDProfile::NORMAL),
          outputMin(0),
          outputMax(PWM_MAX_PID_OUTPUT),  // Use PWM_MAX_PID_OUTPUT as default
          maxTemp(90.0),
          fixedOutput(0),
          computeCallCount(0),
          resetCallCount(0),
          lastSetpoint(0),
          lastInput(0),
          lastTime(0) {
    }

    void begin() override {
        initialized = true;
    }

    void setProfile(PIDProfile profile) override {
        currentProfile = profile;
    }

    void setLimits(float outMin, float outMax) override {
        outputMin = outMin;
        // Cap the max value to PWM_MAX_PID_OUTPUT
        outputMax = (outMax > PWM_MAX_PID_OUTPUT) ? PWM_MAX_PID_OUTPUT : outMax;
    }

    void setMaxAllowedTemp(float maxTemp) override {
        this->maxTemp = maxTemp;
    }

    float compute(float setpoint, float input, uint32_t currentMillis) override {
        computeCallCount++;
        lastSetpoint = setpoint;
        lastInput = input;
        lastTime = currentMillis;

        // Return fixed output or simple proportional for testing
        return constrain(fixedOutput, outputMin, outputMax);
    }

    void reset() override {
        resetCallCount++;
        fixedOutput = 0;
        computeCallCount = 0;
    }

    // Test helpers
    void setOutput(float output) {
        fixedOutput = output;
    }

    bool isInitialized() const { return initialized; }
    PIDProfile getProfile() const { return currentProfile; }
    float getOutputMin() const { return outputMin; }
    float getOutputMax() const { return outputMax; }
    float getMaxTemp() const { return maxTemp; }
    uint32_t getComputeCallCount() const { return computeCallCount; }
    uint32_t getResetCallCount() const { return resetCallCount; }
    float getLastSetpoint() const { return lastSetpoint; }
    float getLastInput() const { return lastInput; }
    uint32_t getLastTime() const { return lastTime; }

    void resetCounts() {
        computeCallCount = 0;
        resetCallCount = 0;
    }

    // Helper function to constrain values (matching real PID behavior)
    float constrain(float value, float min, float max) const {
        if (value < min) return min;
        if (value > max) return max;
        return value;
    }
};

#endif