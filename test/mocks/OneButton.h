#ifndef ONE_BUTTON_MOCK_H
#define ONE_BUTTON_MOCK_H

#include <cstdint>

// Mock OneButton library for testing
class OneButton {
private:
    uint8_t pin;
    bool activeLow;
    bool pressed;

    typedef void (*CallbackFunction)(void);
    CallbackFunction clickCallback;
    CallbackFunction longPressCallback;

    uint32_t debounceMs;
    uint32_t clickMs;
    uint32_t pressMs;

public:
    OneButton(uint8_t pin, bool activeLow = true)
        : pin(pin),
          activeLow(activeLow),
          pressed(false),
          clickCallback(nullptr),
          longPressCallback(nullptr),
          debounceMs(50),
          clickMs(400),
          pressMs(1000) {}

    void setDebounceMs(uint32_t ms) { debounceMs = ms; }
    void setClickMs(uint32_t ms) { clickMs = ms; }
    void setPressMs(uint32_t ms) { pressMs = ms; }

    void attachClick(CallbackFunction callback) { clickCallback = callback; }
    void attachLongPressStart(CallbackFunction callback) { longPressCallback = callback; }

    void tick() {
        // Stub - in real hardware this would read pin state and trigger callbacks
    }

    bool isLongPressed() const { return pressed; }

    // Test helpers (not part of real OneButton API)
    void simulateClick() {
        if (clickCallback) clickCallback();
    }

    void simulateLongPress() {
        if (longPressCallback) longPressCallback();
    }

    void setPressed(bool state) { pressed = state; }
};

#endif
