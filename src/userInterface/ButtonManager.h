#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include "../interfaces/IButtonManager.h"
#include "../Config.h"
#include <OneButton.h>
#include <map>

/**
 * ButtonManager - OneButton wrapper with callback system
 *
 * Manages three physical buttons (SET, UP, DOWN) using OneButton library.
 * Detects single clicks and long presses, fires callbacks per button type.
 */
class ButtonManager : public IButtonManager {
private:
    // OneButton instances for each physical button
    OneButton setButton;
    OneButton upButton;
    OneButton downButton;

    // Callback storage per button type
    std::map<ButtonType, ButtonCallback> callbacks;

    // Static callback wrappers (OneButton requires static functions)
    static ButtonManager* instance;

    static void handleSetClick() {
        if (instance) instance->fireCallback(ButtonType::SET, ButtonEvent::SINGLE_CLICK);
    }

    static void handleSetLongPress() {
        if (instance) instance->fireCallback(ButtonType::SET, ButtonEvent::LONG_PRESS);
    }

    static void handleUpClick() {
        if (instance) instance->fireCallback(ButtonType::UP, ButtonEvent::SINGLE_CLICK);
    }

    static void handleUpLongPress() {
        if (instance) instance->fireCallback(ButtonType::UP, ButtonEvent::LONG_PRESS);
    }

    static void handleDownClick() {
        if (instance) instance->fireCallback(ButtonType::DOWN, ButtonEvent::SINGLE_CLICK);
    }

    static void handleDownLongPress() {
        if (instance) instance->fireCallback(ButtonType::DOWN, ButtonEvent::LONG_PRESS);
    }

    void fireCallback(ButtonType button, ButtonEvent event) {
        auto it = callbacks.find(button);
        if (it != callbacks.end() && it->second) {
            it->second(event);
        }
    }

public:
    ButtonManager()
        : setButton(BUTTON_SET_PIN, true),    // true = INPUT_PULLUP, active LOW
          upButton(BUTTON_UP_PIN, true),
          downButton(BUTTON_DOWN_PIN, true) {
        instance = this;
    }

    void begin() override {
        // Configure timing parameters
        setButton.setDebounceMs(BUTTON_DEBOUNCE_MS);
        setButton.setClickMs(BUTTON_CLICK_MS);
        setButton.setPressMs(BUTTON_LONG_PRESS_MS);

        upButton.setDebounceMs(BUTTON_DEBOUNCE_MS);
        upButton.setClickMs(BUTTON_CLICK_MS);
        upButton.setPressMs(BUTTON_LONG_PRESS_MS);

        downButton.setDebounceMs(BUTTON_DEBOUNCE_MS);
        downButton.setClickMs(BUTTON_CLICK_MS);
        downButton.setPressMs(BUTTON_LONG_PRESS_MS);

        // Attach event handlers
        setButton.attachClick(handleSetClick);
        setButton.attachLongPressStart(handleSetLongPress);

        upButton.attachClick(handleUpClick);
        upButton.attachLongPressStart(handleUpLongPress);

        downButton.attachClick(handleDownClick);
        downButton.attachLongPressStart(handleDownLongPress);
    }

    void update(uint32_t currentMillis) override {
        // Tick all buttons to process state
        setButton.tick();
        upButton.tick();
        downButton.tick();
    }

    void registerButtonCallback(ButtonType button, ButtonCallback callback) override {
        callbacks[button] = callback;
    }

    bool isButtonPressed(ButtonType button) const override {
        switch (button) {
            case ButtonType::SET:
                return setButton.isLongPressed();
            case ButtonType::UP:
                return upButton.isLongPressed();
            case ButtonType::DOWN:
                return downButton.isLongPressed();
            default:
                return false;
        }
    }
};

// Static instance pointer
ButtonManager* ButtonManager::instance = nullptr;

#endif
