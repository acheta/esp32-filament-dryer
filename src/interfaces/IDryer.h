#ifndef I_DRYER_H
#define I_DRYER_H

#include "../Types.h"

/**
 * Interface for Dryer - Main System Orchestrator
 *
 * Responsibilities:
 * - State machine implementation and transitions
 * - Set constraints to components (does NOT enforce them)
 * - Register callbacks with all components
 * - Coordinate system lifecycle
 * - Persist runtime state for power loss recovery
 *
 * Does NOT:
 * - Read sensors directly
 * - Control hardware directly
 * - Handle UI logic
 * - Validate constraints (delegates to components)
 */
class IDryer {
public:
    virtual ~IDryer() = default;

    // Lifecycle
    virtual void begin() = 0;
    virtual void update(uint32_t currentMillis) = 0;

    // State control
    virtual void start() = 0;
    virtual void pause() = 0;
    virtual void resume() = 0;
    virtual void reset() = 0;
    virtual void stop() = 0;

    // Preset management
    virtual void selectPreset(PresetType preset) = 0;
    virtual void setCustomPresetTemp(float temp) = 0;
    virtual void setCustomPresetTime(uint32_t seconds) = 0;
    virtual void setCustomPresetOvershoot(float overshoot) = 0;
    virtual void saveCustomPreset() = 0;
    virtual DryingPreset getCustomPreset() const = 0;

    // PID profile
    virtual void setPIDProfile(PIDProfile profile) = 0;
    virtual PIDProfile getPIDProfile() const = 0;

    // Settings
    virtual void setSoundEnabled(bool enabled) = 0;
    virtual bool isSoundEnabled() const = 0;

    // State queries
    virtual DryerState getState() const = 0;
    virtual CurrentStats getCurrentStats() const = 0;
    virtual PresetType getActivePreset() const = 0;

    // Constraints (for menu controller)
    virtual float getMinTemp() const = 0;
    virtual float getMaxTemp() const = 0;
    virtual uint32_t getMaxTime() const = 0;
    virtual float getMaxOvershoot() const = 0;

    // Callbacks
    virtual void registerStateChangeCallback(StateChangeCallback callback) = 0;
    virtual void registerStatsUpdateCallback(StatsUpdateCallback callback) = 0;
};

#endif