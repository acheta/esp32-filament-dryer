#ifndef SAFETY_MONITOR_H
#define SAFETY_MONITOR_H

#include "../interfaces/ISafetyMonitor.h"
#include "../Config.h"
#include <vector>

/**
 * SafetyMonitor - Passive guardian monitoring temperature limits
 *
 * Monitors sensor readings and triggers emergency stop on violations.
 * Does NOT control heater directly - just notifies via callbacks.
 */
class SafetyMonitor : public ISafetyMonitor {
private:
    float maxHeaterTemp;
    float maxBoxTemp;

    // Last known readings with timestamps
    float lastHeaterTemp;
    uint32_t lastHeaterTimestamp;
    bool heaterValid;

    float lastBoxTemp;
    uint32_t lastBoxTimestamp;
    bool boxValid;

    std::vector<EmergencyStopCallback> emergencyCallbacks;

    bool emergencyTriggered;

    void triggerEmergency(const String& reason) {
        if (emergencyTriggered) {
            return; // Only trigger once
        }

        emergencyTriggered = true;

        for (auto& callback : emergencyCallbacks) {
            if (callback) {
                callback(reason);
            }
        }
    }

public:
    SafetyMonitor()
        : maxHeaterTemp(MAX_HEATER_TEMP),
          maxBoxTemp(MAX_BOX_TEMP),
          lastHeaterTemp(0),
          lastHeaterTimestamp(0),
          heaterValid(false),
          lastBoxTemp(0),
          lastBoxTimestamp(0),
          boxValid(false),
          emergencyTriggered(false) {
    }

    void begin() override {
        emergencyTriggered = false;
    }

    void update(uint32_t currentMillis) override {
        // Check for sensor timeout (only if we had valid readings before)
        if (heaterValid && (currentMillis - lastHeaterTimestamp) > SENSOR_TIMEOUT) {
            triggerEmergency("Heater sensor timeout");
            return;
        }

        if (boxValid && (currentMillis - lastBoxTimestamp) > SENSOR_TIMEOUT) {
            triggerEmergency("Box sensor timeout");
            return;
        }
    }

    void setMaxHeaterTemp(float temp) override {
        maxHeaterTemp = temp;
    }

    void setMaxBoxTemp(float temp) override {
        maxBoxTemp = temp;
    }

    void notifyHeaterTemp(float temp, uint32_t timestamp) override {
        lastHeaterTemp = temp;
        lastHeaterTimestamp = timestamp;
        heaterValid = true;

        if (temp >= maxHeaterTemp) {
            char buffer[100];
            snprintf(buffer, sizeof(buffer), "Heater temp exceeded: %.1fC (max: %.1fC)", temp, maxHeaterTemp);
            triggerEmergency(String(buffer));
        }
    }

    void notifyBoxTemp(float temp, uint32_t timestamp) override {
        lastBoxTemp = temp;
        lastBoxTimestamp = timestamp;
        boxValid = true;

        if (temp >= maxBoxTemp) {
            char buffer[100];
            snprintf(buffer, sizeof(buffer), "Box temp exceeded: %.1fC (max: %.1fC)", temp, maxBoxTemp);
            triggerEmergency(String(buffer));
        }
    }

    void registerEmergencyStopCallback(EmergencyStopCallback callback) override {
        emergencyCallbacks.push_back(callback);
    }
};

#endif