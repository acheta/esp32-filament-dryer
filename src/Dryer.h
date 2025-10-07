#ifndef DRYER_H
#define DRYER_H

#include "interfaces/IDryer.h"
#include "interfaces/ISensorManager.h"
#include "interfaces/IHeaterControl.h"
#include "interfaces/IPIDController.h"
#include "interfaces/ISafetyMonitor.h"
#include "interfaces/ISettingsStorage.h"
#include "interfaces/ISoundController.h"
#include "Types.h"
#include "Config.h"
#include <vector>

/**
 * Dryer - Main System Orchestrator
 *
 * Owns all major components and coordinates their operation through
 * a state machine. Sets constraints but delegates enforcement.
 */
class Dryer : public IDryer {
private:
    // Component dependencies (injected)
    ISensorManager* sensorManager;
    IHeaterControl* heaterControl;
    IPIDController* pidController;
    ISafetyMonitor* safetyMonitor;
    ISettingsStorage* storage;
    ISoundController* soundController;

    // State
    DryerState currentState;
    DryerState previousState;

    // Active configuration
    PresetType activePreset;
    DryingPreset customPreset;
    PIDProfile pidProfile;
    bool soundEnabled;

    // Runtime data
    uint32_t startTime;
    uint32_t pausedTime;
    uint32_t totalPausedDuration;
    uint32_t targetTimeSeconds;
    float targetTemp;
    float maxAllowedTemp;

    // Current readings
    float currentHeaterTemp;
    float currentBoxTemp;
    float currentBoxHumidity;
    float currentPWM;

    // Timing
    uint32_t lastStateSaveTime;
    uint32_t currentTime;  // Updated every update() call

    // Callbacks
    std::vector<StateChangeCallback> stateChangeCallbacks;
    std::vector<StatsUpdateCallback> statsUpdateCallbacks;

    // State transition
    void transitionToState(DryerState newState, uint32_t currentMillis) {
        if (currentState == newState) return;

        previousState = currentState;
        currentState = newState;

        // Notify callbacks
        for (auto& callback : stateChangeCallbacks) {
            if (callback) {
                callback(previousState, currentState);
            }
        }

        // Handle state entry actions
        onStateEnter(newState, previousState, currentMillis);
    }

    void onStateEnter(DryerState newState, DryerState prevState, uint32_t currentMillis) {
        switch (newState) {
            case DryerState::READY:
                heaterControl->stop(currentMillis);
                pidController->reset();
                break;

            case DryerState::RUNNING:
                heaterControl->start(currentMillis);
                if (prevState == DryerState::READY) {
                    // Fresh start
                    startTime = currentMillis;
                    totalPausedDuration = 0;
                } else if (prevState == DryerState::PAUSED) {
                    // Resuming from pause
                    totalPausedDuration += (currentMillis - pausedTime);
                }
                break;

            case DryerState::PAUSED:
                heaterControl->stop(currentMillis);
                pausedTime = currentMillis;
                break;

            case DryerState::FINISHED:
                heaterControl->stop(currentMillis);
                pidController->reset();
                storage->clearRuntimeState();
                if (soundEnabled && soundController) {
                    soundController->playFinished();
                }
                break;

            case DryerState::FAILED:
                heaterControl->emergencyStop();
                pidController->reset();
                if (soundEnabled && soundController) {
                    soundController->playAlarm();
                }
                break;

            case DryerState::POWER_RECOVERED:
                heaterControl->stop(currentMillis);
                break;
        }
    }

    void setupCallbacks() {
        // Register with sensor manager
        sensorManager->registerHeaterTempCallback(
            [this](float temp, uint32_t timestamp) {
                onHeaterTempUpdate(temp, timestamp);
            }
        );

        sensorManager->registerBoxDataCallback(
            [this](float temp, float humidity, uint32_t timestamp) {
                onBoxDataUpdate(temp, humidity, timestamp);
            }
        );

        sensorManager->registerSensorErrorCallback(
            [this](SensorType type, const String& error) {
                onSensorError(type, error);
            }
        );

        // Register with safety monitor
        safetyMonitor->registerEmergencyStopCallback(
            [this](const String& reason) {
                onEmergencyStop(reason);
            }
        );
    }

    void onHeaterTempUpdate(float temp, uint32_t timestamp) {
        currentHeaterTemp = temp;

        // If running, update PID and heater
        if (currentState == DryerState::RUNNING) {
            float output = pidController->compute(targetTemp, temp, timestamp);
            currentPWM = output;
            heaterControl->setPWM((uint8_t)output);
        }
    }

    void onBoxDataUpdate(float temp, float humidity, uint32_t timestamp) {
        currentBoxTemp = temp;
        currentBoxHumidity = humidity;
    }

    void onSensorError(SensorType type, const String& error) {
        // Sensor errors are handled by SafetyMonitor
        // This is just for logging/display purposes
    }

    void onEmergencyStop(const String& reason) {
        // Emergency uses currentTime since it's triggered during update loop
        transitionToState(DryerState::FAILED, currentTime);
        storage->saveEmergencyState(reason);
    }

    void loadPreset(PresetType preset) {
        switch (preset) {
            case PresetType::PLA:
                targetTemp = PRESET_PLA_TEMP;
                targetTimeSeconds = PRESET_PLA_TIME;
                maxAllowedTemp = targetTemp + PRESET_PLA_OVERSHOOT;
                break;

            case PresetType::PETG:
                targetTemp = PRESET_PETG_TEMP;
                targetTimeSeconds = PRESET_PETG_TIME;
                maxAllowedTemp = targetTemp + PRESET_PETG_OVERSHOOT;
                break;

            case PresetType::CUSTOM:
                targetTemp = customPreset.targetTemp;
                targetTimeSeconds = customPreset.targetTime;
                maxAllowedTemp = targetTemp + customPreset.maxOvershoot;
                break;
        }

        activePreset = preset;

        // Update component constraints
        pidController->setMaxAllowedTemp(maxAllowedTemp);
        safetyMonitor->setMaxBoxTemp(MAX_BOX_TEMP);
        safetyMonitor->setMaxHeaterTemp(maxAllowedTemp);
    }

    void persistState(uint32_t currentMillis) {
        if (currentState != DryerState::RUNNING) return;

        if (currentMillis - lastStateSaveTime >= STATE_SAVE_INTERVAL) {
            lastStateSaveTime = currentMillis;

            uint32_t elapsed = getElapsedTime(currentMillis);
            storage->saveRuntimeState(
                currentState,
                elapsed,
                targetTemp,
                targetTimeSeconds,
                activePreset,
                currentMillis
            );
        }
    }

    void notifyStatsUpdate(uint32_t currentMillis) {
        CurrentStats stats = getCurrentStats(currentMillis);
        for (auto& callback : statsUpdateCallbacks) {
            if (callback) {
                callback(stats);
            }
        }
    }

    /** Calculate elapsed time in seconds */
    uint32_t getElapsedTime(uint32_t currentMillis) const {
        if (currentState == DryerState::RUNNING) {
            return ((currentMillis - startTime) - totalPausedDuration)/1000;
        } else if (currentState == DryerState::PAUSED) {
            return ((pausedTime - startTime) - totalPausedDuration)/1000;
        }
        return 0;
    }

    CurrentStats getCurrentStats(uint32_t currentMillis) const {
        CurrentStats stats;
        stats.state = currentState;
        stats.currentTemp = currentHeaterTemp;
        stats.targetTemp = targetTemp;
        stats.boxTemp = currentBoxTemp;
        stats.boxHumidity = currentBoxHumidity;
        stats.elapsedTime = getElapsedTime(currentMillis);
        stats.remainingTime = (targetTimeSeconds > stats.elapsedTime) ?
                              (targetTimeSeconds - stats.elapsedTime) : 0;
        stats.pwmOutput = currentPWM;
        stats.activePreset = activePreset;
        return stats;
    }

public:
    Dryer(ISensorManager* sensors,
          IHeaterControl* heater,
          IPIDController* pid,
          ISafetyMonitor* safety,
          ISettingsStorage* store,
          ISoundController* sound = nullptr)
        : sensorManager(sensors),
          heaterControl(heater),
          pidController(pid),
          safetyMonitor(safety),
          storage(store),
          soundController(sound),
          currentState(DryerState::READY),
          previousState(DryerState::READY),
          activePreset(PresetType::PLA),
          pidProfile(PIDProfile::NORMAL),
          soundEnabled(true),
          startTime(0),
          pausedTime(0),
          totalPausedDuration(0),
          targetTimeSeconds(PRESET_PLA_TIME),
          targetTemp(PRESET_PLA_TEMP),
          maxAllowedTemp(PRESET_PLA_TEMP + PRESET_PLA_OVERSHOOT),
          currentHeaterTemp(0),
          currentBoxTemp(0),
          currentBoxHumidity(0),
          currentPWM(0),
          lastStateSaveTime(0),
          currentTime(0) {

        // Initialize custom preset with defaults
        customPreset.targetTemp = PRESET_CUSTOM_TEMP;
        customPreset.targetTime = PRESET_CUSTOM_TIME;
        customPreset.maxOvershoot = PRESET_CUSTOM_OVERSHOOT;
    }

    void begin(uint32_t currentMillis) override {
        // Initialize all components
        sensorManager->begin();
        heaterControl->begin(currentMillis);
        pidController->begin();
        safetyMonitor->begin();
        storage->begin();

        if (soundController) {
            soundController->begin();
            soundController->setEnabled(soundEnabled);
        }

        // Setup callbacks
        setupCallbacks();

        // Load settings
        storage->loadSettings();

        // Try to recover from power loss
        if (storage->hasValidRuntimeState()) {
            // Load runtime state
            // Transition to POWER_RECOVERED
            transitionToState(DryerState::POWER_RECOVERED, 0);
        } else {
            // Normal startup
            loadPreset(activePreset);
            setPIDProfile(pidProfile);
        }
    }

    void update(uint32_t currentMillis) override {
        // Store current time for use in user-triggered actions
        currentTime = currentMillis;

        // Update all components
        sensorManager->update(currentMillis);
        safetyMonitor->update(currentMillis);

        // State-specific updates
        if (currentState == DryerState::RUNNING) {
            // Check if target time reached
            uint32_t elapsed = getElapsedTime(currentMillis);
            if (elapsed >= targetTimeSeconds) {
                transitionToState(DryerState::FINISHED, currentMillis);
            }

            // Persist state periodically
            persistState(currentMillis);
        }

        // Notify stats update (for display)
        notifyStatsUpdate(currentMillis);
    }

    void start() override {
        if (currentState == DryerState::READY ||
            currentState == DryerState::POWER_RECOVERED) {
            transitionToState(DryerState::RUNNING, currentTime);
            if (soundEnabled && soundController) {
                soundController->playStart();
            }
        }
    }

    void pause() override {
        if (currentState == DryerState::RUNNING) {
            transitionToState(DryerState::PAUSED, currentTime);
        }
    }

    void resume() override {
        if (currentState == DryerState::PAUSED) {
            transitionToState(DryerState::RUNNING, currentTime);
        }
    }

    void reset() override {
        transitionToState(DryerState::READY, currentTime);
        startTime = 0;
        pausedTime = 0;
        totalPausedDuration = 0;
        storage->clearRuntimeState();
    }

    void stop() override {
        if (currentState == DryerState::RUNNING ||
            currentState == DryerState::PAUSED) {
            transitionToState(DryerState::READY, currentTime);
        }
    }

    void selectPreset(PresetType preset) override {
        if (currentState == DryerState::READY ||
            currentState == DryerState::POWER_RECOVERED) {
            loadPreset(preset);
        }
    }

    void setCustomPresetTemp(float temp) override {
        customPreset.targetTemp = constrain(temp, MIN_TEMP, MAX_BOX_TEMP);
    }

    void setCustomPresetTime(uint32_t seconds) override {
        customPreset.targetTime = constrain(seconds, MIN_TIME_SECONDS, MAX_TIME_SECONDS);
    }

    void setCustomPresetOvershoot(float overshoot) override {
        customPreset.maxOvershoot = constrain(overshoot, 0.0f, DEFAULT_MAX_OVERSHOOT);
    }

    void saveCustomPreset() override {
        storage->saveCustomPreset(customPreset);
    }

    DryingPreset getCustomPreset() const override {
        return customPreset;
    }

    void setPIDProfile(PIDProfile profile) override {
        pidProfile = profile;
        pidController->setProfile(profile);
    }

    PIDProfile getPIDProfile() const override {
        return pidProfile;
    }

    void setSoundEnabled(bool enabled) override {
        soundEnabled = enabled;
        if (soundController) {
            soundController->setEnabled(enabled);
        }
        storage->saveSoundEnabled(enabled);
    }

    bool isSoundEnabled() const override {
        return soundEnabled;
    }

    DryerState getState() const override {
        return currentState;
    }

    CurrentStats getCurrentStats() const override {
        return getCurrentStats(currentTime);
    }

    PresetType getActivePreset() const override {
        return activePreset;
    }

    float getMinTemp() const override {
        return MIN_TEMP;
    }

    float getMaxTemp() const override {
        return MAX_BOX_TEMP;
    }

    uint32_t getMaxTime() const override {
        return MAX_TIME_SECONDS;
    }

    float getMaxOvershoot() const override {
        return DEFAULT_MAX_OVERSHOOT;
    }

    void registerStateChangeCallback(StateChangeCallback callback) override {
        stateChangeCallbacks.push_back(callback);
    }

    void registerStatsUpdateCallback(StatsUpdateCallback callback) override {
        statsUpdateCallbacks.push_back(callback);
    }
};

#endif