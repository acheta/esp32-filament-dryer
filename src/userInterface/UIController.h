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
 *
 * Optimizations:
 * - Only updates display when values actually change
 * - Uses stored currentTime instead of millis() for consistency
 * - Dirty flag prevents unnecessary display refreshes
 */
class UIController {
private:
    IDisplay* display;
    IMenuController* menuController;
    IButtonManager* buttonManager;
    ISoundController* soundController;
    IDryer* dryer;

    // Timing - stored from update() for use in callbacks
    uint32_t currentTime;

    // UI state
    enum class UIMode {
        HOME,      // Show stats screens
        MENU       // Show menu
    };

    UIMode currentMode;

    // HOME screen stats cycling
    enum class StatsScreen {
        BOX_TEMP,           // Default: large box temp with "B:" prefix
        REMAINING,          // Large remaining time in h:mm:ss format
        HEATER_TEMP,        // Large heater temp with "H:" prefix
        STATUS_OVERVIEW,    // State, Elapsed, Fan, Sound (all size 1)
        PRESET_CONFIG,      // Preset, PID, Temp/Overshoot, Target time (all size 1)
        SENSOR_READINGS     // Box, Heater, PID/PWM_MAX, Humidity (all size 1)
    };

    StatsScreen currentStatsScreen;

    // Inactivity timeout for menu
    uint32_t lastMenuActivity;
    static constexpr uint32_t MENU_TIMEOUT_MS = 30000;  // 30 seconds

    // Last stats for display
    CurrentStats lastStats;

    // Cached display values to detect changes (for HOME mode optimization)
    struct CachedDisplayValues {
        float boxTemp;
        float heaterTemp;
        float boxHumidity;
        uint32_t remainingTime;
        DryerState state;
        PresetType preset;

        CachedDisplayValues()
            : boxTemp(-999), heaterTemp(-999), boxHumidity(-999),
              remainingTime(0), state(DryerState::READY), preset(PresetType::PLA) {}

        bool operator!=(const CachedDisplayValues& other) const {
            return (abs(boxTemp - other.boxTemp) > 0.05f) ||
                   (abs(heaterTemp - other.heaterTemp) > 0.05f) ||
                   (abs(boxHumidity - other.boxHumidity) > 0.5f) ||
                   (remainingTime != other.remainingTime) ||
                   (state != other.state) ||
                   (preset != other.preset);
        }
    };

    CachedDisplayValues cachedValues;

    // Dirty flag for display optimization
    bool displayNeedsUpdate;

    void setupButtonCallbacks() {
        if (!buttonManager) {
            Serial.println("ERROR: Cannot setup button callbacks - buttonManager is null!");
            return;
        }

        Serial.println("    Registering SET button...");
        // SET button
        buttonManager->registerButtonCallback(ButtonType::SET,
            [this](ButtonEvent event) {
                lastMenuActivity = currentTime;  // Use stored currentTime
                displayNeedsUpdate = true;

                if (currentMode == UIMode::HOME) {
                    if (event == ButtonEvent::SINGLE_CLICK) {
                        // Enter menu - update remaining time before entering
                        menuController->setRemainingTime(lastStats.remainingTime);
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
                lastMenuActivity = currentTime;  // Use stored currentTime
                displayNeedsUpdate = true;

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
                lastMenuActivity = currentTime;  // Use stored currentTime
                displayNeedsUpdate = true;

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

                // Only update display in HOME mode if displayed values actually changed
                if (currentMode == UIMode::HOME) {
                    CachedDisplayValues newValues;
                    newValues.boxTemp = stats.boxTemp;
                    newValues.heaterTemp = stats.currentTemp;
                    newValues.boxHumidity = stats.boxHumidity;
                    newValues.remainingTime = stats.remainingTime;
                    newValues.state = stats.state;
                    newValues.preset = stats.activePreset;

                    if (newValues != cachedValues) {
                        cachedValues = newValues;
                        displayNeedsUpdate = true;
                    }
                }
                // In MENU mode, don't set dirty flag from stats updates
                // Only button presses or menu actions trigger display updates
            }
        );
    }

    void enterMenu() {
        currentMode = UIMode::MENU;
        menuController->reset();
        lastMenuActivity = currentTime;  // Use stored currentTime
        displayNeedsUpdate = true;
    }

    void exitMenu() {
        currentMode = UIMode::HOME;
        displayNeedsUpdate = true;
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
        displayNeedsUpdate = true;
    }

    void cycleStatsScreen(bool forward) {
        int current = (int)currentStatsScreen;

        if (forward) {
            current++;
            if (current > (int)StatsScreen::SENSOR_READINGS) {
                current = 0;
            }
        } else {
            current--;
            if (current < 0) {
                current = (int)StatsScreen::SENSOR_READINGS;
            }
        }

        currentStatsScreen = (StatsScreen)current;
        displayNeedsUpdate = true;
    }

    void handleMenuSelection(MenuPath path, int value) {
        displayNeedsUpdate = true;

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

            case MenuPath::ADJUST_TIMER: {
                uint32_t newRemainingTimeSeconds = value * 60;
                uint32_t currentRemainingTimeSeconds = lastStats.remainingTime;
                int32_t deltaSeconds = (int32_t)newRemainingTimeSeconds - (int32_t)currentRemainingTimeSeconds;
                dryer->adjustRemainingTime(deltaSeconds);

                // Refresh stats and update menu
                lastStats = dryer->getCurrentStats();
                menuController->setRemainingTime(lastStats.remainingTime);

                if (soundController) soundController->playConfirm();
                break;
            }

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

            case MenuPath::BACK:
                // Check if we're at the root menu
                if (menuController->getCurrentMenuPath() == MenuPath::ROOT) {
                    // Exit menu to home screen
                    exitMenu();
                }
                // Otherwise, navigateBack() was already called in MenuController
                break;

            default:
                break;
        }
    }

    String getPresetAbbreviation(PresetType preset) const {
        switch (preset) {
            case PresetType::PLA: return "PLA";
            case PresetType::PETG: return "PETG";
            case PresetType::CUSTOM: return "CUST";
            default: return "???";
        }
    }

    void renderHomeScreen() {
        display->clear();

        // Handle different screen types
        switch (currentStatsScreen) {
            case StatsScreen::BOX_TEMP:
            case StatsScreen::HEATER_TEMP:
            case StatsScreen::REMAINING:
                renderLargValueScreen();
                break;

            case StatsScreen::STATUS_OVERVIEW:
                renderStatusOverviewScreen();
                break;

            case StatsScreen::PRESET_CONFIG:
                renderPresetConfigScreen();
                break;

            case StatsScreen::SENSOR_READINGS:
                renderSensorReadingsScreen();
                break;
        }

        display->display();
    }

    void renderLargValueScreen() {
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

        // Line 1.1 (Y=0): State indicator (left) + Preset name (right)
        display->setCursor(0, 0);
        display->setTextSize(1);
        display->print(String(stateChar));

        // Preset name in top-right corner
        display->setCursor(128 - (4 * 6), 0);  // 4 chars * 6 pixels wide
        display->print(getPresetAbbreviation(lastStats.activePreset));

        // Lines 1.2 + 2 (Y=8, spanning 16px height): Main value with prefix
        switch (currentStatsScreen) {
            case StatsScreen::BOX_TEMP:
                // Show "B:" label on line 1.2
                display->setCursor(0, 8);
                display->setTextSize(1);
                display->print("B:");

                // Show temperature value (large)
                display->setTextSize(2);
                display->setCursor(14, 0);
                display->print(String(lastStats.boxTemp, 1));
                display->setTextSize(1);
                display->print("C");
                break;

            case StatsScreen::HEATER_TEMP:
                // Show "H:" label on line 1.2
                display->setCursor(0, 8);
                display->setTextSize(1);
                display->print("H:");

                // Show temperature value (large)
                display->setTextSize(2);
                display->setCursor(14, 0);
                display->print(String(lastStats.currentTemp, 1));
                display->setTextSize(1);
                display->print("C");
                break;

            case StatsScreen::REMAINING:
                // Show remaining time in h:mm:ss format (large)
                display->setTextSize(2);
                display->setCursor(14, 0);
                {
                    uint32_t hrs = lastStats.remainingTime / 3600;
                    uint32_t mins = (lastStats.remainingTime % 3600) / 60;
                    uint32_t secs = lastStats.remainingTime % 60;

                    display->print(String(hrs));
                    display->print(":");
                    if (mins < 10) display->print("0");
                    display->print(String(mins));
                    display->print(":");
                    if (secs < 10) display->print("0");
                    display->print(String(secs));
                }
                break;

            default:
                break;
        }

        // Line 2 (Y=16): Remaining time countdown (left) + PWM (right)
        if (lastStats.state == DryerState::RUNNING ||
            lastStats.state == DryerState::PAUSED) {
            display->setTextSize(1);
            display->setCursor(0, 16);

            // Remaining time as mm:ss
            uint32_t remainMin = lastStats.remainingTime / 60;
            uint32_t remainSec = lastStats.remainingTime % 60;
            display->print(String(remainMin));
            display->print(":");
            if (remainSec < 10) display->print("0");
            display->print(String(remainSec));
            // PWM output on the right
            display->print(" PW:");
            display->print(String((int)lastStats.pwmOutput));
            // Heater temp on the right
            display->setCursor(128 - (8 * 6), 16);  // Approx "H:51.2C" = 8 chars
            display->print("H:");
            display->print(String(lastStats.currentTemp, 1));
            display->print("C");
        }

        // Line 3 (Y=24): "B:xx.xC xx% /xx°C" (box temp + humidity + target)
        display->setTextSize(1);
        display->setCursor(0, 24);
        display->print("B:");
        display->print(String(lastStats.boxTemp, 1));
        display->print("C ");
        display->print(String(lastStats.boxHumidity, 0));
        display->print("% /");
        display->print(String(lastStats.targetTemp, 0));
        display->print("C");
    }

    void renderStatusOverviewScreen() {
        display->setTextSize(1);

        // Line 0 (Y=0): State
        display->setCursor(0, 0);
        display->print("State: ");
        switch (lastStats.state) {
            case DryerState::READY: display->print("READY"); break;
            case DryerState::RUNNING: display->print("RUNNING"); break;
            case DryerState::PAUSED: display->print("PAUSED"); break;
            case DryerState::FINISHED: display->print("FINISHED"); break;
            case DryerState::FAILED: display->print("FAILED"); break;
            case DryerState::POWER_RECOVERED: display->print("POWER_REC"); break;
        }

        // Line 1 (Y=8): Elapsed time
        display->setCursor(0, 8);
        display->print("Elapsed: ");
        uint32_t hrs = lastStats.elapsedTime / 3600;
        uint32_t mins = (lastStats.elapsedTime % 3600) / 60;
        uint32_t secs = lastStats.elapsedTime % 60;
        display->print(String(hrs));
        display->print(":");
        if (mins < 10) display->print("0");
        display->print(String(mins));
        display->print(":");
        if (secs < 10) display->print("0");
        display->print(String(secs));

        // Line 2 (Y=16): Fan status
        display->setCursor(0, 16);
        display->print("Fan: ");
        display->print(lastStats.fanRunning ? "ON" : "OFF");

        // Line 3 (Y=24): Sound status
        display->setCursor(0, 24);
        display->print("Sound: ");
        display->print(dryer->isSoundEnabled() ? "ON" : "OFF");
    }

    void renderPresetConfigScreen() {
        display->setTextSize(1);

        // Line 0 (Y=0): Preset
        display->setCursor(0, 0);
        display->print("Preset: ");
        switch (lastStats.activePreset) {
            case PresetType::PLA: display->print("PLA"); break;
            case PresetType::PETG: display->print("PETG"); break;
            case PresetType::CUSTOM: display->print("CUSTOM"); break;
        }

        // Line 1 (Y=8): PID profile
        display->setCursor(0, 8);
        display->print("PID: ");
        switch (lastStats.pidProfile) {
            case PIDProfile::SOFT: display->print("SOFT"); break;
            case PIDProfile::NORMAL: display->print("NORMAL"); break;
            case PIDProfile::STRONG: display->print("STRONG"); break;
        }

        // Line 2 (Y=16): Temp/Overshoot
        display->setCursor(0, 16);
        display->print("Temp/Ovr: ");
        display->print(String((int)lastStats.targetTemp));
        display->print("/");
        display->print(String((int)lastStats.maxOvershoot));
        display->print("C");

        // Line 3 (Y=24): Target time
        display->setCursor(0, 24);
        display->print("Target: ");
        uint32_t hrs = lastStats.targetTime / 3600;
        uint32_t mins = (lastStats.targetTime % 3600) / 60;
        uint32_t secs = lastStats.targetTime % 60;
        display->print(String(hrs));
        display->print(":");
        if (mins < 10) display->print("0");
        display->print(String(mins));
        display->print(":");
        if (secs < 10) display->print("0");
        display->print(String(secs));
    }

    void renderSensorReadingsScreen() {
        display->setTextSize(1);

        // Line 0 (Y=0): Box temp
        display->setCursor(0, 0);
        display->print("Box: ");
        display->print(String(lastStats.boxTemp, 1));
        display->print("C");

        // Line 1 (Y=8): Heater temp
        display->setCursor(0, 8);
        display->print("Heater: ");
        display->print(String(lastStats.currentTemp, 1));
        display->print("C");

        // Line 2 (Y=16): PID output
        display->setCursor(0, 16);
        display->print("PID: ");
        display->print(String((int)lastStats.pwmOutput));
        display->print("/");
        display->print(String(PWM_MAX));

        // Line 3 (Y=24): Humidity
        display->setCursor(0, 24);
        display->print("Humidity: ");
        display->print(String(lastStats.boxHumidity, 0));
        display->print("%");
    }

    void renderMenuScreen() {
        display->clear();

        if (menuController->isInEditMode()) {
            // Edit mode: show item label and value (centered)
            MenuItem item = menuController->getEditingItem();
            int value = menuController->getEditValue();

            display->setTextSize(1);
            display->setCursor(0, 4);
            display->print(item.label);

            display->setCursor(0, 16);
            display->setTextSize(2);
            display->print(String(value));
            display->setTextSize(1);
            display->print(item.unit);
        }
        else if (menuController->getCurrentMenuPath() == MenuPath::SYSTEM_INFO) {
            // System info: show as scrollable label + value list
            renderSystemInfoScreen();
        }
        else {
            // Navigation mode: show menu items
            std::vector<MenuItem> items = menuController->getCurrentMenuItems();
            int selection = menuController->getCurrentSelection();

            if (items.empty()) {
                display->display();
                return;
            }

            // Show 3 items: prev, current (large), next
            int prevIndex = selection - 1;
            if (prevIndex < 0) prevIndex = items.size() - 1;

            int nextIndex = selection + 1;
            if (nextIndex >= (int)items.size()) nextIndex = 0;

            // Previous item (small, top) - Y=0
            display->setTextSize(1);
            display->setCursor(0, 0);
            display->print(items[prevIndex].label);

            if (items[prevIndex].type == MenuItemType::VALUE_EDIT) {
                display->print(": ");
                display->print(String(items[prevIndex].currentValue));
                display->print(items[prevIndex].unit);
            }

            // Current item (large, middle) - Y=8
            display->setTextSize(2);
            display->setCursor(0, 8);
            display->print(items[selection].label);

            if (items[selection].type == MenuItemType::VALUE_EDIT) {
                display->setTextSize(1);
                display->print(": ");
                display->setTextSize(2);
                display->print(String(items[selection].currentValue));
                display->setTextSize(1);
                display->print(items[selection].unit);
            }

            // Next item (small, bottom) - Y=24
            display->setTextSize(1);
            display->setCursor(0, 24);
            display->print(items[nextIndex].label);

            if (items[nextIndex].type == MenuItemType::VALUE_EDIT) {
                display->print(": ");
                display->print(String(items[nextIndex].currentValue));
                display->print(items[nextIndex].unit);
            }
        }

        display->display();
    }

    void renderSystemInfoScreen() {
        // System info displays as: label (line 1) + value+unit (line 2, large)
        std::vector<MenuItem> items = menuController->getCurrentMenuItems();
        int selection = menuController->getCurrentSelection();

        if (items.empty() || selection >= (int)items.size()) {
            display->display();
            return;
        }

        MenuItem currentItem = items[selection];

        // Check if this is the "Back" item
        if (currentItem.path == MenuPath::BACK) {
            // Show "Back" as a regular menu item
            display->setTextSize(2);
            display->setCursor(0, 8);
            display->print("Back");
        } else {
            // Show info item: label + value
            display->setTextSize(1);
            display->setCursor(0, 4);
            display->print(currentItem.label);

            display->setTextSize(2);
            display->setCursor(0, 16);
            display->print(String(currentItem.currentValue));
            display->setTextSize(1);
            display->print(currentItem.unit);
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
          currentTime(0),
          currentMode(UIMode::HOME),
          currentStatsScreen(StatsScreen::BOX_TEMP),
          lastMenuActivity(0),
          displayNeedsUpdate(true) {
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

        // Initialize cached values
        cachedValues.boxTemp = lastStats.boxTemp;
        cachedValues.heaterTemp = lastStats.currentTemp;
        cachedValues.boxHumidity = lastStats.boxHumidity;
        cachedValues.remainingTime = lastStats.remainingTime;
        cachedValues.state = lastStats.state;
        cachedValues.preset = lastStats.activePreset;

        Serial.println("UIController::begin() - Initialization complete!");
    }

    void update(uint32_t currentMillis) {
        // Store current time for use in callbacks
        currentTime = currentMillis;

        // Update buttons (high priority - must be called frequently)
        buttonManager->update(currentMillis);

        // Check menu timeout
        checkMenuTimeout(currentMillis);

        // Only render if display needs update (dirty flag optimization)
        if (displayNeedsUpdate) {
            if (currentMode == UIMode::HOME) {
                renderHomeScreen();
            } else {
                renderMenuScreen();
            }
            displayNeedsUpdate = false;  // Clear dirty flag
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