#ifndef MOCK_HEATER_CONTROL_H
#define MOCK_HEATER_CONTROL_H

#include "../../src/interfaces/IHeaterControl.h"

class MockHeaterControl : public IHeaterControl {
private:
    bool initialized;
    bool running;
    uint8_t pwmValue;
    uint32_t startCallCount;
    uint32_t stopCallCount;
    uint32_t emergencyStopCallCount;
    uint32_t setPWMCallCount;
    bool emergencyStopped;

public:
    MockHeaterControl()
        : initialized(false),
          running(false),
          pwmValue(0),
          startCallCount(0),
          stopCallCount(0),
          emergencyStopCallCount(0),
          setPWMCallCount(0),
          emergencyStopped(false) {
    }

    void begin(uint32_t currentMillis) override {
        initialized = true;
    }

    void start(uint32_t currentMillis) override {
        startCallCount++;
        running = true;
        emergencyStopped = false;
    }

    void stop(uint32_t currentMillis) override {
        stopCallCount++;
        running = false;
        pwmValue = 0;
    }

    void emergencyStop() override {
        emergencyStopCallCount++;
        running = false;
        pwmValue = 0;
        emergencyStopped = true;
    }

    void setPWM(uint8_t value) override {
        setPWMCallCount++;
        if (!running) {
            pwmValue = 0;
        } else {
            pwmValue = value;
        }
    }

    bool isRunning() const override {
        return running;
    }

    uint8_t getCurrentPWM() const override {
        return pwmValue;
    }

    void update(uint32_t currentMillis) override {
        // Mock doesn't need to do anything
    }

    // Test helpers
    bool isInitialized() const { return initialized; }
    uint32_t getStartCallCount() const { return startCallCount; }
    uint32_t getStopCallCount() const { return stopCallCount; }
    uint32_t getEmergencyStopCallCount() const { return emergencyStopCallCount; }
    uint32_t getSetPWMCallCount() const { return setPWMCallCount; }
    bool wasEmergencyStopped() const { return emergencyStopped; }
    
    void resetCounts() {
        startCallCount = 0;
        stopCallCount = 0;
        emergencyStopCallCount = 0;
        setPWMCallCount = 0;
    }
};

#endif