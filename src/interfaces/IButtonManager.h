#ifndef I_BUTTON_MANAGER_H
#define I_BUTTON_MANAGER_H

#include "../Types.h"

/**
 * Interface for ButtonManager
 *
 * Responsibilities:
 * - Wrap OneButton library for each physical button
 * - Detect single click and long press events
 * - Fire button-specific callbacks
 * - Handle debouncing and timing
 *
 * Does NOT:
 * - Interpret button meaning (UI controller does this)
 * - Know about menu state
 * - Execute actions
 */
class IButtonManager {
public:
    virtual ~IButtonManager() = default;

    // Lifecycle
    virtual void begin() = 0;
    virtual void update(uint32_t currentMillis) = 0;

    // Callback registration
    virtual void registerButtonCallback(ButtonType button, ButtonCallback callback) = 0;

    // State queries (for debugging)
    virtual bool isButtonPressed(ButtonType button) const = 0;
};

#endif