#ifndef MOCK_DRYER_H
#define MOCK_DRYER_H

#include "../../src/interfaces/IDryer.h"
#include <vector>

/**
 * MockDryer - Test double for IDryer
 *
 * Allows manual control of state and tracking of method calls
 * for testing UI and other components that depend on Dryer.
 */
class MockDryer : public IDryer {
private:
    DryerState currentState;
    PresetType activePreset;
    DryingPreset customPreset;
    PIDProfile pidProfile;
    bool soundEnabled;
    CurrentStats stats;

    std::vector<StateChangeCallback> stateCallbacks;
    std::vector<StatsUpdateCallback> statsCallbacks;

    // Call tracking
    uint32_t beginCallCount;
    uint32_t updateCallCount;
    uint32_t startCallCount;
    uint32_t pauseCallCount;
    uint32_t resumeCallCount;
    uint32_t resetCallCount;
    uint32_t stopCallCount;

public:
    MockDryer()
        : currentState(DryerState::READY),
          activePreset(PresetType::PLA),
          pidProfile(PIDProfile::NORMAL),
          soundEnabled(true),
          beginCallCount(0),
          updateCallCount(0),
          startCallCount(0),
          pauseCallCount(0),
          resumeCallCount(0),
          resetCallCount(0),
          stopCallCount(0) {

        customPreset.targetTemp = 50.0;
        customPreset.targetTime = 14400;
        customPreset.maxOvershoot = 10.0;

        stats.state = currentState;
        stats.currentTemp = 0;
        stats.targetTemp = 50.0;
        stats.boxTemp = 0;
        stats.boxHumidity = 0;
        stats.elapsedTime = 0;
        stats.remainingTime = 14400;
        stats.pwmOutput = 0;
        stats.activePreset = activePreset;
    }

    void begin() override {
        beginCallCount++;
    }

    void update(uint32_t currentMillis) override {
        updateCallCount++;
    }

    void start() override {
        startCallCount++;
        if (currentState == DryerState::READY ||
            currentState == DryerState::POWER_RECOVERED) {
            setState(DryerState::RUNNING);
        }
    }

    void pause() override {
        pauseCallCount++;
        if (currentState == DryerState::RUNNING) {
            setState(DryerState::PAUSED);
        }
    }

    void resume() override {
        resumeCallCount++;
        if (currentState == DryerState::PAUSED) {
            setState(DryerState::RUNNING);
        }
    }

    void reset() override {
        resetCallCount++;
        setState(DryerState::READY);
        stats.elapsedTime = 0;
        stats.remainingTime = customPreset.targetTime;
    }

    void stop() override {
        stopCallCount++;
        setState(DryerState::READY);
    }

    void selectPreset(PresetType preset) override {
        activePreset = preset;
        stats.activePreset = preset;
    }

    void setCustomPresetTemp(float temp) override {
        customPreset.targetTemp = temp;
    }

    void setCustomPresetTime(uint32_t seconds) override {
        customPreset.targetTime = seconds;
    }

    void setCustomPresetOvershoot(float overshoot) override {
        customPreset.maxOvershoot = overshoot;
    }

    void saveCustomPreset() override {
        // Mock does nothing
    }

    DryingPreset getCustomPreset() const override {
        return customPreset;
    }

    void setPIDProfile(PIDProfile profile) override {
        pidProfile = profile;
    }

    PIDProfile getPIDProfile() const override {
        return pidProfile;
    }

    void setSoundEnabled(bool enabled) override {
        soundEnabled = enabled;
    }

    bool isSoundEnabled() const override {
        return soundEnabled;
    }

    DryerState getState() const override {
        return currentState;
    }

    CurrentStats getCurrentStats() const override {
        return stats;
    }

    PresetType getActivePreset() const override {
        return activePreset;
    }

    float getMinTemp() const override {
        return 30.0;
    }

    float getMaxTemp() const override {
        return 80.0;
    }

    uint32_t getMaxTime() const override {
        return 36000;
    }

    float getMaxOvershoot() const override {
        return 10.0;
    }

    void registerStateChangeCallback(StateChangeCallback callback) override {
        stateCallbacks.push_back(callback);
    }

    void registerStatsUpdateCallback(StatsUpdateCallback callback) override {
        statsCallbacks.push_back(callback);
    }

    // ==================== Test Helper Methods ====================

    void setState(DryerState newState) {
        DryerState oldState = currentState;
        currentState = newState;
        stats.state = newState;

        for (auto& callback : stateCallbacks) {
            if (callback) {
                callback(oldState, newState);
            }
        }
    }

    void setStats(const CurrentStats& newStats) {
        stats = newStats;
    }

    void triggerStatsUpdate() {
        for (auto& callback : statsCallbacks) {
            if (callback) {
                callback(stats);
            }
        }
    }

    uint32_t getBeginCallCount() const { return beginCallCount; }
    uint32_t getUpdateCallCount() const { return updateCallCount; }
    uint32_t getStartCallCount() const { return startCallCount; }
    uint32_t getPauseCallCount() const { return pauseCallCount; }
    uint32_t getResumeCallCount() const { return resumeCallCount; }
    uint32_t getResetCallCount() const { return resetCallCount; }
    uint32_t getStopCallCount() const { return stopCallCount; }

    size_t getStateCallbackCount() const { return stateCallbacks.size(); }
    size_t getStatsCallbackCount() const { return statsCallbacks.size(); }
};

#endif