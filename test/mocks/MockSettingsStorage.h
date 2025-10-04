#ifndef MOCK_SETTINGS_STORAGE_H
#define MOCK_SETTINGS_STORAGE_H

#include "../../src/interfaces/ISettingsStorage.h"

class MockSettingsStorage : public ISettingsStorage {
private:
    bool initialized;
    DryingPreset customPreset;
    bool soundEnabled;
    bool hasRuntimeState;
    DryerState savedState;
    uint32_t savedElapsed;

    uint32_t beginCallCount;
    uint32_t saveSettingsCallCount;
    uint32_t loadSettingsCallCount;
    uint32_t saveCustomPresetCallCount;
    uint32_t saveRuntimeStateCallCount;
    uint32_t clearRuntimeStateCallCount;

public:
    MockSettingsStorage()
        : initialized(false),
          soundEnabled(true),
          hasRuntimeState(false),
          savedState(DryerState::READY),
          savedElapsed(0),
          beginCallCount(0),
          saveSettingsCallCount(0),
          loadSettingsCallCount(0),
          saveCustomPresetCallCount(0),
          saveRuntimeStateCallCount(0),
          clearRuntimeStateCallCount(0) {

        customPreset.targetTemp = 50.0;
        customPreset.targetTime = 14400;
        customPreset.maxOvershoot = 10.0;
    }

    void begin() override {
        initialized = true;
        beginCallCount++;
    }

    void loadSettings() override {
        loadSettingsCallCount++;
    }

    void saveSettings() override {
        saveSettingsCallCount++;
    }

    void saveCustomPreset(const DryingPreset& preset) override {
        saveCustomPresetCallCount++;
        customPreset = preset;
    }

    DryingPreset loadCustomPreset() override {
        return customPreset;
    }

    void saveSoundEnabled(bool enabled) override {
        soundEnabled = enabled;
    }

    bool loadSoundEnabled() override {
        return soundEnabled;
    }

    void saveRuntimeState(DryerState state, uint32_t elapsed,
                         float targetTemp, uint32_t targetTime,
                         PresetType preset, uint32_t timestamp) override {
        saveRuntimeStateCallCount++;
        hasRuntimeState = true;
        savedState = state;
        savedElapsed = elapsed;
    }

    bool hasValidRuntimeState() override {
        return hasRuntimeState;
    }

    void loadRuntimeState() override {
        // Mock doesn't actually restore state
    }

    void clearRuntimeState() override {
        clearRuntimeStateCallCount++;
        hasRuntimeState = false;
    }

    void saveEmergencyState(const String& reason) override {
        // Mock just saves as runtime state
        hasRuntimeState = true;
        savedState = DryerState::FAILED;
    }

    // Test helpers
    bool isInitialized() const { return initialized; }
    uint32_t getBeginCallCount() const { return beginCallCount; }
    uint32_t getSaveSettingsCallCount() const { return saveSettingsCallCount; }
    uint32_t getLoadSettingsCallCount() const { return loadSettingsCallCount; }
    uint32_t getSaveCustomPresetCallCount() const { return saveCustomPresetCallCount; }
    uint32_t getSaveRuntimeStateCallCount() const { return saveRuntimeStateCallCount; }
    uint32_t getClearRuntimeStateCallCount() const { return clearRuntimeStateCallCount; }

    void setHasRuntimeState(bool has) { hasRuntimeState = has; }

    void resetCounts() {
        beginCallCount = 0;
        saveSettingsCallCount = 0;
        loadSettingsCallCount = 0;
        saveCustomPresetCallCount = 0;
        saveRuntimeStateCallCount = 0;
        clearRuntimeStateCallCount = 0;
    }
};

#endif