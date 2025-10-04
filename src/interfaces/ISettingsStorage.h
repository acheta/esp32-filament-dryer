#ifndef I_SETTINGS_STORAGE_H
#define I_SETTINGS_STORAGE_H

#include "../Types.h"
#ifndef UNIT_TEST
    #include <Arduino.h>
#else
    // Mock Arduino functions for native testing
    #include "../../test/mocks/arduino_mock.h"
#endif

class ISettingsStorage {
public:
    virtual ~ISettingsStorage() = default;
    virtual void begin() = 0;
    virtual void loadSettings() = 0;
    virtual void saveSettings() = 0;
    virtual void saveCustomPreset(const DryingPreset& preset) = 0;
    virtual DryingPreset loadCustomPreset() = 0;
    virtual void saveSoundEnabled(bool enabled) = 0;
    virtual bool loadSoundEnabled() = 0;
    virtual void saveRuntimeState(DryerState state, uint32_t elapsed,
                                   float targetTemp, uint32_t targetTime,
                                   PresetType preset, uint32_t timestamp) = 0;
    virtual bool hasValidRuntimeState() = 0;
    virtual void loadRuntimeState() = 0;
    virtual void clearRuntimeState() = 0;
    virtual void saveEmergencyState(const String& reason) = 0;
};

#endif