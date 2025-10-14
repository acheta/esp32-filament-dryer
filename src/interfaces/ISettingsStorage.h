
#ifndef I_SETTINGS_STORAGE_H
#define I_SETTINGS_STORAGE_H

#include "../Types.h"
#ifndef UNIT_TEST
    #include <Arduino.h>
#else
    #include "../../test/mocks/arduino_mock.h"
#endif

/**
 * Interface for Settings Storage
 *
 * Responsibilities:
 * - Persist user settings (custom preset, selected preset, PID profile, sound)
 * - Save/restore runtime state for power recovery
 * - Handle corruption and graceful degradation
 *
 * Storage Organization:
 * - Settings: Custom preset, selected preset, PID profile, sound enabled
 * - Runtime: Current cycle state for power loss recovery
 */
class ISettingsStorage {
public:
    virtual ~ISettingsStorage() = default;

    // Lifecycle
    virtual void begin() = 0;
    virtual void loadSettings() = 0;
    virtual void saveSettings() = 0;

    // Custom preset
    virtual void saveCustomPreset(const DryingPreset& preset) = 0;
    virtual DryingPreset loadCustomPreset() = 0;

    // Selected preset (for persistence)
    virtual void saveSelectedPreset(PresetType preset) = 0;
    virtual PresetType loadSelectedPreset() = 0;

    // PID profile (for persistence)
    virtual void savePIDProfile(PIDProfile profile) = 0;
    virtual PIDProfile loadPIDProfile() = 0;

    // Sound setting
    virtual void saveSoundEnabled(bool enabled) = 0;
    virtual bool loadSoundEnabled() = 0;

    // Runtime state (for power recovery)
    virtual void saveRuntimeState(DryerState state, uint32_t elapsed,
                                   float targetTemp, uint32_t targetTime,
                                   PresetType preset, uint32_t timestamp) = 0;
    virtual bool hasValidRuntimeState() = 0;
    virtual void loadRuntimeState() = 0;
    virtual void clearRuntimeState() = 0;
    virtual void saveEmergencyState(const String& reason) = 0;

    // Runtime state accessors (for power recovery)
    virtual DryerState getRuntimeState() const = 0;
    virtual uint32_t getRuntimeElapsed() const = 0;
    virtual float getRuntimeTargetTemp() const = 0;
    virtual uint32_t getRuntimeTargetTime() const = 0;
    virtual PresetType getRuntimePreset() const = 0;
};

#endif


