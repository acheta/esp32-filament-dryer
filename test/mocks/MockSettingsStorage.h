
#ifndef MOCK_SETTINGS_STORAGE_H
#define MOCK_SETTINGS_STORAGE_H

#include "../../src/interfaces/ISettingsStorage.h"

class MockSettingsStorage : public ISettingsStorage {
private:
    bool initialized;
    DryingPreset customPreset;
    PresetType selectedPreset;
    PIDProfile selectedPIDProfile;
    bool soundEnabled;
    bool hasRuntimeState;
    DryerState savedState;
    uint32_t savedElapsed;
    float savedTargetTemp;
    uint32_t savedTargetTime;
    PresetType savedPreset;

    uint32_t beginCallCount;
    uint32_t saveSettingsCallCount;
    uint32_t loadSettingsCallCount;
    uint32_t saveCustomPresetCallCount;
    uint32_t saveSelectedPresetCallCount;
    uint32_t savePIDProfileCallCount;
    uint32_t saveRuntimeStateCallCount;
    uint32_t clearRuntimeStateCallCount;

public:
    MockSettingsStorage()
        : initialized(false),
          selectedPreset(PresetType::PLA),
          selectedPIDProfile(PIDProfile::NORMAL),
          soundEnabled(true),
          hasRuntimeState(false),
          savedState(DryerState::READY),
          savedElapsed(0),
          savedTargetTemp(50.0),
          savedTargetTime(14400),
          savedPreset(PresetType::PLA),
          beginCallCount(0),
          saveSettingsCallCount(0),
          loadSettingsCallCount(0),
          saveCustomPresetCallCount(0),
          saveSelectedPresetCallCount(0),
          savePIDProfileCallCount(0),
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

    void saveSelectedPreset(PresetType preset) override {
        saveSelectedPresetCallCount++;
        selectedPreset = preset;
    }

    PresetType loadSelectedPreset() override {
        return selectedPreset;
    }

    void savePIDProfile(PIDProfile profile) override {
        savePIDProfileCallCount++;
        selectedPIDProfile = profile;
    }

    PIDProfile loadPIDProfile() override {
        return selectedPIDProfile;
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
        savedTargetTemp = targetTemp;
        savedTargetTime = targetTime;
        savedPreset = preset;
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

    DryerState getRuntimeState() const override {
        return savedState;
    }

    uint32_t getRuntimeElapsed() const override {
        return savedElapsed;
    }

    float getRuntimeTargetTemp() const override {
        return savedTargetTemp;
    }

    uint32_t getRuntimeTargetTime() const override {
        return savedTargetTime;
    }

    PresetType getRuntimePreset() const override {
        return savedPreset;
    }

    // Test helpers
    bool isInitialized() const { return initialized; }
    bool isHealthy() const { return true; }  // Mock always healthy
    String getLastError() const { return ""; }
    String getInitErrorMessage() const { return ""; }

    uint32_t getBeginCallCount() const { return beginCallCount; }
    uint32_t getSaveSettingsCallCount() const { return saveSettingsCallCount; }
    uint32_t getLoadSettingsCallCount() const { return loadSettingsCallCount; }
    uint32_t getSaveCustomPresetCallCount() const { return saveCustomPresetCallCount; }
    uint32_t getSaveSelectedPresetCallCount() const { return saveSelectedPresetCallCount; }
    uint32_t getSavePIDProfileCallCount() const { return savePIDProfileCallCount; }
    uint32_t getSaveRuntimeStateCallCount() const { return saveRuntimeStateCallCount; }
    uint32_t getClearRuntimeStateCallCount() const { return clearRuntimeStateCallCount; }

    void setHasRuntimeState(bool has) { hasRuntimeState = has; }

    void setSelectedPreset(PresetType preset) { selectedPreset = preset; }
    void setPIDProfile(PIDProfile profile) { selectedPIDProfile = profile; }
    void setSoundEnabled(bool enabled) { soundEnabled = enabled; }
    void setCustomPreset(const DryingPreset& preset) { customPreset = preset; }

    void setRuntimeState(DryerState state, uint32_t elapsed,
                        float targetTemp, uint32_t targetTime,
                        PresetType preset) {
        hasRuntimeState = true;
        savedState = state;
        savedElapsed = elapsed;
        savedTargetTemp = targetTemp;
        savedTargetTime = targetTime;
        savedPreset = preset;
    }

    void resetCounts() {
        beginCallCount = 0;
        saveSettingsCallCount = 0;
        loadSettingsCallCount = 0;
        saveCustomPresetCallCount = 0;
        saveSelectedPresetCallCount = 0;
        savePIDProfileCallCount = 0;
        saveRuntimeStateCallCount = 0;
        clearRuntimeStateCallCount = 0;
    }
};

#endif


