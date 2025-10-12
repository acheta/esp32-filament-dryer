#ifndef I_MENU_CONTROLLER_H
#define I_MENU_CONTROLLER_H

#include "../Types.h"
#include <vector>

/**
 * Interface for MenuController
 *
 * Responsibilities:
 * - Menu state machine and navigation
 * - Value editing for custom presets
 * - Menu item generation for display
 * - Fire callbacks on user selections
 *
 * Does NOT:
 * - Execute actions (delegates via callbacks)
 * - Persist data
 * - Know about dryer internals
 */
class IMenuController {
public:
    virtual ~IMenuController() = default;

    // Navigation
    virtual void handleAction(MenuAction action) = 0;
    virtual void reset() = 0;

    // Menu state queries
    virtual std::vector<MenuItem> getCurrentMenuItems() = 0;
    virtual MenuPath getCurrentMenuPath() const = 0;
    virtual int getCurrentSelection() const = 0;
    virtual bool isInEditMode() const = 0;

    // Edit mode queries
    virtual MenuItem getEditingItem() const = 0;
    virtual int getEditValue() const = 0;

    // Configuration
    virtual void setConstraints(float minTemp, float maxTemp,
                                uint32_t maxTime, float maxOvershoot) = 0;
    virtual void setCustomPresetValues(float temp, uint32_t time, float overshoot) = 0;
    virtual void setPIDProfile(const String& profile) = 0;
    virtual void setSoundEnabled(bool enabled) = 0;
    virtual void setRemainingTime(uint32_t seconds) = 0;

    // Callbacks
    virtual void registerSelectionCallback(MenuSelectionCallback callback) = 0;
};

#endif