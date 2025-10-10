#ifndef UI_CONTROLLER_H
#define UI_CONTROLLER_H

#include "../interfaces/IDisplay.h"
#include "../interfaces/IMenuController.h"
#include "../interfaces/IButtonManager.h"
#include "../interfaces/ISoundController.h"
#include "../interfaces/IDryer.h"
#include "../Config.h"

/**
 * UIController - Main UI coordinator
 *
 * Coordinates between buttons, menu, display, and dryer.
 * Handles different screen modes (HOME stats vs MENU).
 */
class UIController {
private:
    IDisplay* display;
    IMenuController* menuController;
    IButtonManager* buttonManager;
    ISoundController* soundController;
    IDryer* dryer;

    // UI state
    enum class UIMode {
        HOME,      // Show stats screens
        MENU       // Show menu
    };

    UIMode currentMode;

    // HOME screen stats cycling
    enum class StatsScreen {
        BOX_TEMP,      // Default: large box temp
        REMAINING,     // Large remaining time
        HEATER_TEMP,   // Large heater temp
        PID_OUTPUT     // Large PID output
    };

    StatsScreen currentStatsScreen;

    // Inactivity timeout for menu
    uint32_t lastMenuActivity;
    static constexpr uint32_t MENU_TIMEOUT_MS = 30000;  // 30 seconds

    // Last stats for display
    CurrentStats lastStats;

    void setupButtonCallbacks() {
        if (!buttonManager) {
            Serial.println("ERROR: Cannot setup button callbacks - buttonManager is null!");
            return;
        }

        Serial.println("    Registering SET button...");
        // SET button
        buttonManager->registerButtonCallback(ButtonType::SET,
            [this](ButtonEvent event) {
                lastMenuActivity = millis();

                if (currentMode == UIMode::HOME) {
                    if (event == ButtonEvent::SINGLE_CLICK) {
                        // Enter menu
                        enterMenu();
                        if (soundController) soundController->playClick();
                    } else if (event == ButtonEvent::LONG_PRESS) {
                        // Toggle pause/resume
                        handleHomeLongPress();
                        if (soundController) soundController->playClick();
                    }
                } else {  // MENU mode
                    if (event == ButtonEvent::SINGLE_CLICK) {
                        menuController->handleAction(MenuAction::ENTER);
                        if (soundController) soundController->playClick();
                    } else if (event == ButtonEvent::LONG_PRESS) {
                        // Go back in menu (or exit at root)
                        if (menuController->getCurrentMenuPath() == MenuPath::ROOT) {
                            exitMenu();
                        } else {
                            menuController->handleAction(MenuAction::BACK);
                        }
                        if (soundController) soundController->playClick();
                    }
                }
            }
        );

        Serial.println("    Registering UP button...");
        // UP button
        buttonManager->registerButtonCallback(ButtonType::UP,
            [this](ButtonEvent event) {
                lastMenuActivity = millis();

                if (event == ButtonEvent::SINGLE_CLICK) {
                    if (currentMode == UIMode::HOME) {
                        // Cycle stats screens
                        cycleStatsScreen(true);
                        if (soundController) soundController->playClick();
                    } else {
                        menuController->handleAction(MenuAction::UP);
                        if (soundController) soundController->playClick();
                    }
                }
            }
        );

        Serial.println("    Registering DOWN button...");
        // DOWN button
        buttonManager->registerButtonCallback(ButtonType::DOWN,
            [this](ButtonEvent event) {
                lastMenuActivity = millis();

                if (event == ButtonEvent::SINGLE_CLICK) {
                    if (currentMode == UIMode::HOME) {
                        // Cycle stats screens
                        cycleStatsScreen(false);
                        if (soundController) soundController->playClick();
                    } else {
                        menuController->handleAction(MenuAction::DOWN);
                        if (soundController) soundController->playClick();
                    }
                }
            }
        );

        Serial.println("    Button callbacks registered successfully!");
    }

    void setupMenuCallbacks() {
        menuController->registerSelectionCallback(
            [this](MenuPath path, int value) {
                handleMenuSelection(path, value);
            }
        );
    }

    void setupDryerCallbacks() {
        dryer->registerStatsUpdateCallback(
            [this](const CurrentStats& stats) {
                lastStats = stats;
            }
        );
    }

    void enterMenu() {
        currentMode = UIMode::MENU;
        menuController->reset();
        lastMenuActivity = millis();
    }

    void exitMenu() {
        currentMode = UIMode::HOME;
    }

    void handleHomeLongPress() {
        DryerState state = dryer->getState();

        if (state == DryerState::RUNNING) {
            dryer->pause();
        } else if (state == DryerState::PAUSED) {
            dryer->resume();
        } else if (state == DryerState::READY || state == DryerState::POWER_RECOVERED) {
            dryer->start();
        }
    }

    void cycleStatsScreen(bool forward) {
        int current = (int)currentStatsScreen;

        if (forward) {
            current++;
            if (current > (int)StatsScreen::PID_OUTPUT) {
                current = 0;
            }
        } else {
            current--;
            if (current < 0) {
                current = (int)StatsScreen::PID_OUTPUT;
            }
        }

        currentStatsScreen = (StatsScreen)current;
    }

    void handleMenuSelection(MenuPath path, int value) {
        switch (path) {
            case MenuPath::STATUS_START:
                dryer->start();
                if (soundController) soundController->playStart();
                exitMenu();
                break;

            case MenuPath::STATUS_PAUSE:
                dryer->pause();
                exitMenu();
                break;

            case MenuPath::STATUS_RESET:
                dryer->reset();
                exitMenu();
                break;

            case MenuPath::PRESET_PLA:
                dryer->selectPreset(PresetType::PLA);
                if (soundController) soundController->playConfirm();
                exitMenu();
                break;

            case MenuPath::PRESET_PETG:
                dryer->selectPreset(PresetType::PETG);
                if (soundController) soundController->playConfirm();
                exitMenu();
                break;

            case MenuPath::PRESET_CUSTOM:
                dryer->selectPreset(PresetType::CUSTOM);
                if (soundController) soundController->playConfirm();
                exitMenu();
                break;

            case MenuPath::CUSTOM_TEMP:
                dryer->setCustomPresetTemp(value);
                break;

            case MenuPath::CUSTOM_TIME:
                dryer->setCustomPresetTime(value * 60);  // Convert minutes to seconds
                break;

            case MenuPath::CUSTOM_OVERSHOOT:
                dryer->setCustomPresetOvershoot(value);
                break;

            case MenuPath::CUSTOM_SAVE:
                dryer->saveCustomPreset();
                if (soundController) soundController->playConfirm();
                break;

            case MenuPath::CUSTOM_COPY_PLA:
                // Copy PLA preset values to custom
                dryer->setCustomPresetTemp(PRESET_PLA_TEMP);
                dryer->setCustomPresetTime(PRESET_PLA_TIME);
                dryer->setCustomPresetOvershoot(PRESET_PLA_OVERSHOOT);
                menuController->setCustomPresetValues(
                    PRESET_PLA_TEMP,
                    PRESET_PLA_TIME,
                    PRESET_PLA_OVERSHOOT
                );
                if (soundController) soundController->playConfirm();
                break;

            case MenuPath::PID_SOFT:
                dryer->setPIDProfile(PIDProfile::SOFT);
                menuController->setPIDProfile("SOFT");
                if (soundController) soundController->playConfirm();
                exitMenu();
                break;

            case MenuPath::PID_NORMAL:
                dryer->setPIDProfile(PIDProfile::NORMAL);
                menuController->setPIDProfile("NORMAL");
                if (soundController) soundController->playConfirm();
                exitMenu();
                break;

            case MenuPath::PID_STRONG:
                dryer->setPIDProfile(PIDProfile::STRONG);
                menuController->setPIDProfile("STRONG");
                if (soundController) soundController->playConfirm();
                exitMenu();
                break;

            case MenuPath::SOUND_ON:
                dryer->setSoundEnabled(true);
                menuController->setSoundEnabled(true);
                if (soundController) soundController->playConfirm();
                break;

            case MenuPath::SOUND_OFF:
                dryer->setSoundEnabled(false);
                menuController->setSoundEnabled(false);
                break;

            default:
                break;
        }
    }

    void renderHomeScreen() {
        display->clear();

        // Get state character
        char stateChar = 'R';  // Default READY
        switch (lastStats.state) {
            case DryerState::READY: stateChar = 'R'; break;
            case DryerState::RUNNING: stateChar = '>'; break;
            case DryerState::PAUSED: stateChar = '|'; break;
            case DryerState::FINISHED: stateChar = 'F'; break;
            case DryerState::FAILED: stateChar = '!'; break;
            case DryerState::POWER_RECOVERED: stateChar = 'P'; break;
        }

        // Line 1: State indicator (top-left, small)
        display->setCursor(0, 0);
        display->setTextSize(1);
        display->print(String(stateChar));

        // Line 1: Main value (large, centered)
        display->setTextSize(2);
        display->setCursor(24, 0);

        switch (currentStatsScreen) {
            case StatsScreen::BOX_TEMP:
                display->print(String(lastStats.boxTemp, 1));
                display->setTextSize(1);
                display->print("C");
                break;

            case StatsScreen::REMAINING:
                {
                    uint32_t remainMin = lastStats.remainingTime / 60;
                    display->print(String(remainMin));
                    display->setTextSize(1);
                    display->print("m");
                }
                break;

            case StatsScreen::HEATER_TEMP:
                display->print(String(lastStats.currentTemp, 1));
                display->setTextSize(1);
                display->print("C");
                break;

            case StatsScreen::PID_OUTPUT:
                display->print(String((int)lastStats.pwmOutput));
                display->setTextSize(1);
                display->print("%");
                break;
        }

        // Line 1: Target temp (if running/paused, right side)
        if (lastStats.state == DryerState::RUNNING ||
            lastStats.state == DryerState::PAUSED) {
            display->setTextSize(1);
            display->setCursor(90, 0);
            display->print("/");
            display->print(String(lastStats.targetTemp, 0));
            display->print("C");
        }

        // Line 2: Timer and PWM (if running/paused)
        display->setTextSize(1);
        display->setCursor(0, 16);

        if (lastStats.state == DryerState::RUNNING ||
            lastStats.state == DryerState::PAUSED) {
            // Elapsed time
            uint32_t elapsedMin = lastStats.elapsedTime / 60;
            uint32_t elapsedSec = lastStats.elapsedTime % 60;
            display->print(String(elapsedMin));
            display->print(":");
            if (elapsedSec < 10) display->print("0");
            display->print(String(elapsedSec));

            display->print(" /");
            uint32_t remainMin = lastStats.remainingTime / 60;
            display->print(String(remainMin));
            display->print("m");

            // PWM
            display->print(" P:");
            display->print(String((int)lastStats.pwmOutput));
        } else {
            // Show heater temp when not running
            display->print("Heater:");
            display->print(String(lastStats.currentTemp, 1));
            display->print("C");
        }

        // Line 3: Box data (always shown, smaller)
        display->setCursor(0, 24);
        display->setTextSize(1);
        display->print("B:");
        display->print(String(lastStats.boxTemp, 1));
        display->print("C ");
        display->print(String(lastStats.boxHumidity, 0));
        display->print("%");

        display->display();
    }

    void renderMenuScreen() {
        display->clear();

        if (menuController->isInEditMode()) {
            // Edit mode: show item label and value
            MenuItem item = menuController->getEditingItem();
            int value = menuController->getEditValue();

            // Center-aligned
            display->setTextSize(1);
            display->setCursor(0, 8);
            display->print(item.label);

            display->setCursor(0, 20);
            display->setTextSize(2);
            display->print(String(value));
            display->setTextSize(1);
            display->print(item.unit);

        } else {
            // Navigation mode: show menu items
            std::vector<MenuItem> items = menuController->getCurrentMenuItems();
            int selection = menuController->getCurrentSelection();

            if (items.empty()) {
                display->display();
                return;
            }

            // Show 3 items: prev, current (large), next
            // Left-aligned, endless loop

            int prevIndex = selection - 1;
            if (prevIndex < 0) prevIndex = items.size() - 1;

            int nextIndex = selection + 1;
            if (nextIndex >= (int)items.size()) nextIndex = 0;

            // Previous item (small, top)
            display->setTextSize(1);
            display->setCursor(0, 0);
            display->print(items[prevIndex].label);

            // For value edit items, show current value
            if (items[prevIndex].type == MenuItemType::VALUE_EDIT) {
                display->print(": ");
                display->print(String(items[prevIndex].currentValue));
                display->print(items[prevIndex].unit);
            }

            // Current item (large, middle)
            display->setTextSize(2);
            display->setCursor(0, 12);
            display->print(items[selection].label);

            // For value edit items, show current value
            if (items[selection].type == MenuItemType::VALUE_EDIT) {
                display->setTextSize(1);
                display->print(": ");
                display->setTextSize(2);
                display->print(String(items[selection].currentValue));
                display->setTextSize(1);
                display->print(items[selection].unit);
            }

            // Next item (small, bottom)
            display->setTextSize(1);
            display->setCursor(0, 28);
            display->print(items[nextIndex].label);

            // For value edit items, show current value
            if (items[nextIndex].type == MenuItemType::VALUE_EDIT) {
                display->print(": ");
                display->print(String(items[nextIndex].currentValue));
                display->print(items[nextIndex].unit);
            }
        }

        display->display();
    }

    void checkMenuTimeout(uint32_t currentMillis) {
        if (currentMode == UIMode::MENU) {
            if (currentMillis - lastMenuActivity >= MENU_TIMEOUT_MS) {
                exitMenu();
            }
        }
    }

public:
    UIController(IDisplay* disp,
                 IMenuController* menu,
                 IButtonManager* buttons,
                 ISoundController* sound,
                 IDryer* dryerInstance)
        : display(disp),
          menuController(menu),
          buttonManager(buttons),
          soundController(sound),
          dryer(dryerInstance),
          currentMode(UIMode::HOME),
          currentStatsScreen(StatsScreen::BOX_TEMP),
          lastMenuActivity(0) {
    }

    void begin() {
        // Validate all required components are present
        if (!display) {
            Serial.println("ERROR: Display is null!");
            return;
        }
        if (!menuController) {
            Serial.println("ERROR: MenuController is null!");
            return;
        }
        if (!buttonManager) {
            Serial.println("ERROR: ButtonManager is null!");
            return;
        }
        if (!dryer) {
            Serial.println("ERROR: Dryer is null!");
            return;
        }

        Serial.println("UIController::begin() - Starting initialization...");

        // Pass constraints from Dryer to MenuController
        Serial.println("  Setting constraints...");
        menuController->setConstraints(
            dryer->getMinTemp(),
            dryer->getMaxTemp(),
            dryer->getMaxTime(),
            dryer->getMaxOvershoot()
        );

        // Initialize menu with current custom preset values
        Serial.println("  Setting custom preset values...");
        DryingPreset preset = dryer->getCustomPreset();
        menuController->setCustomPresetValues(
            preset.targetTemp,
            preset.targetTime,
            preset.maxOvershoot
        );

        // Set initial PID profile name
        Serial.println("  Setting PID profile...");
        switch (dryer->getPIDProfile()) {
            case PIDProfile::SOFT:
                menuController->setPIDProfile("SOFT");
                break;
            case PIDProfile::NORMAL:
                menuController->setPIDProfile("NORMAL");
                break;
            case PIDProfile::STRONG:
                menuController->setPIDProfile("STRONG");
                break;
        }

        // Set sound state
        Serial.println("  Setting sound state...");
        menuController->setSoundEnabled(dryer->isSoundEnabled());

        // Setup callbacks
        Serial.println("  Setting up button callbacks...");
        setupButtonCallbacks();

        Serial.println("  Setting up menu callbacks...");
        setupMenuCallbacks();

        Serial.println("  Setting up dryer callbacks...");
        setupDryerCallbacks();

        // Initialize display with current stats
        Serial.println("  Getting initial stats...");
        lastStats = dryer->getCurrentStats();

        Serial.println("UIController::begin() - Initialization complete!");
    }

    void update(uint32_t currentMillis) {
        // Update buttons
        buttonManager->update(currentMillis);

        // Check menu timeout
        checkMenuTimeout(currentMillis);

        // Render appropriate screen
        if (currentMode == UIMode::HOME) {
            renderHomeScreen();
        } else {
            renderMenuScreen();
        }
    }

    // Getters for debugging
    bool isInMenuMode() const {
        return currentMode == UIMode::MENU;
    }

    StatsScreen getCurrentStatsScreen() const {
        return currentStatsScreen;
    }
};

#endif