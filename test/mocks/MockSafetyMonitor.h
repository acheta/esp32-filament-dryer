#ifndef MOCK_SAFETY_MONITOR_H
#define MOCK_SAFETY_MONITOR_H

#include "../../src/interfaces/ISafetyMonitor.h"
#include <vector>

class MockSafetyMonitor : public ISafetyMonitor {
private:
    bool initialized;
    float maxHeaterTemp;
    float maxBoxTemp;
    uint32_t updateCallCount;
    std::vector<EmergencyStopCallback> callbacks;
    bool shouldTriggerEmergency;
    String emergencyReason;

public:
    MockSafetyMonitor()
        : initialized(false),
          maxHeaterTemp(90.0),
          maxBoxTemp(80.0),
          updateCallCount(0),
          shouldTriggerEmergency(false) {
    }

    void begin() override {
        initialized = true;
    }

    void update(uint32_t currentMillis) override {
        updateCallCount++;

        if (shouldTriggerEmergency) {
            for (auto& callback : callbacks) {
                if (callback) {
                    callback(emergencyReason);
                }
            }
            shouldTriggerEmergency = false;
        }
    }

    void setMaxHeaterTemp(float temp) override {
        maxHeaterTemp = temp;
    }

    void setMaxBoxTemp(float temp) override {
        maxBoxTemp = temp;
    }

    void notifyHeaterTemp(float temp, uint32_t timestamp) override {
        // Mock doesn't check limits
    }

    void notifyBoxTemp(float temp, uint32_t timestamp) override {
        // Mock doesn't check limits
    }

    void registerEmergencyStopCallback(EmergencyStopCallback callback) override {
        callbacks.push_back(callback);
    }

    // Test helpers
    void triggerEmergency(const String& reason) {
        shouldTriggerEmergency = true;
        emergencyReason = reason;
    }

    bool isInitialized() const { return initialized; }
    float getMaxHeaterTemp() const { return maxHeaterTemp; }
    float getMaxBoxTemp() const { return maxBoxTemp; }
    uint32_t getUpdateCallCount() const { return updateCallCount; }
    size_t getCallbackCount() const { return callbacks.size(); }

    void resetCounts() {
        updateCallCount = 0;
    }
};

#endif