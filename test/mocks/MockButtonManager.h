#ifndef MOCK_BUTTON_MANAGER_H
#define MOCK_BUTTON_MANAGER_H

#include "../../src/interfaces/IButtonManager.h"
#include <map>
#include <vector>

/**
 * MockButtonManager - Test double for ButtonManager
 *
 * Allows tests to:
 * - Simulate button press events
 * - Verify callbacks are registered
 * - Track callback invocations
 */
class MockButtonManager : public IButtonManager {
private:
    std::map<ButtonType, ButtonCallback> callbacks;
    std::map<ButtonType, bool> buttonStates;  // For isButtonPressed

    // Call tracking
    struct CallRecord {
        ButtonType button;
        ButtonEvent event;
    };
    std::vector<CallRecord> callHistory;

    bool beginCalled;
    int updateCallCount;

public:
    MockButtonManager()
        : beginCalled(false), updateCallCount(0) {
        buttonStates[ButtonType::SET] = false;
        buttonStates[ButtonType::UP] = false;
        buttonStates[ButtonType::DOWN] = false;
    }

    void begin() override {
        beginCalled = true;
    }

    void update(uint32_t currentMillis) override {
        updateCallCount++;
    }

    void registerButtonCallback(ButtonType button, ButtonCallback callback) override {
        callbacks[button] = callback;
    }

    bool isButtonPressed(ButtonType button) const override {
        auto it = buttonStates.find(button);
        if (it != buttonStates.end()) {
            return it->second;
        }
        return false;
    }

    // ====================  Test Helper Methods ====================

    /**
     * Simulate a button event (fires registered callback)
     */
    void simulateButtonEvent(ButtonType button, ButtonEvent event) {
        // Record the call
        callHistory.push_back({button, event});

        // Fire callback if registered
        auto it = callbacks.find(button);
        if (it != callbacks.end() && it->second) {
            it->second(event);
        }
    }

    /**
     * Simulate single click
     */
    void simulateClick(ButtonType button) {
        simulateButtonEvent(button, ButtonEvent::SINGLE_CLICK);
    }

    /**
     * Simulate long press
     */
    void simulateLongPress(ButtonType button) {
        simulateButtonEvent(button, ButtonEvent::LONG_PRESS);
    }

    /**
     * Set button pressed state (for isButtonPressed queries)
     */
    void setButtonPressed(ButtonType button, bool pressed) {
        buttonStates[button] = pressed;
    }

    /**
     * Check if callback was registered for a button
     */
    bool hasCallbackFor(ButtonType button) const {
        return callbacks.find(button) != callbacks.end() && callbacks.at(button) != nullptr;
    }

    /**
     * Get number of callbacks registered
     */
    size_t getCallbackCount() const {
        size_t count = 0;
        for (const auto& pair : callbacks) {
            if (pair.second) count++;
        }
        return count;
    }

    /**
     * Check if begin was called
     */
    bool wasBeginCalled() const {
        return beginCalled;
    }

    /**
     * Get update call count
     */
    int getUpdateCallCount() const {
        return updateCallCount;
    }

    /**
     * Get call history size
     */
    size_t getCallHistorySize() const {
        return callHistory.size();
    }

    /**
     * Get specific call from history
     */
    CallRecord getCall(size_t index) const {
        if (index < callHistory.size()) {
            return callHistory[index];
        }
        return {ButtonType::SET, ButtonEvent::SINGLE_CLICK};  // Default
    }

    /**
     * Clear call history
     */
    void clearCallHistory() {
        callHistory.clear();
    }

    /**
     * Reset mock state
     */
    void reset() {
        callbacks.clear();
        buttonStates[ButtonType::SET] = false;
        buttonStates[ButtonType::UP] = false;
        buttonStates[ButtonType::DOWN] = false;
        callHistory.clear();
        beginCalled = false;
        updateCallCount = 0;
    }
};

#endif
