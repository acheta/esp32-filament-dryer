#ifndef MENU_CONTROLLER_H
#define MENU_CONTROLLER_H

#include "../interfaces/IMenuController.h"
#include "../Config.h"
#include <vector>

/**
 * MenuController - Menu state machine with value editing
 *
 * Manages menu navigation, item selection, and value editing.
 * Provides menu items for display and fires callbacks on selections.
 */
class MenuController : public IMenuController {
private:
    // Current menu state
    MenuPath currentMenu;
    int currentSelection;
    bool inEditMode;

    // Edit state
    MenuItem editingItem;
    int editValue;

    // Constraints (from Dryer)
    float minTemp;
    float maxTemp;
    uint32_t maxTime;
    float maxOvershoot;

    // Current custom preset draft (maintained during editing)
    struct CustomPresetDraft {
        float temp;
        uint32_t time;
        float overshoot;
    } customDraft;

    // Current PID profile name (for display)
    String currentPIDProfile;

    // Current sound state (for display)
    bool soundEnabled;

    // Current remaining time for adjust timer (in seconds)
    uint32_t currentRemainingTime;

    // Callbacks
    std::vector<MenuSelectionCallback> callbacks;

    // Menu navigation history (for back navigation)
    std::vector<MenuPath> menuHistory;

    void notifyCallbacks(MenuPath path, int value) {
        for (auto& callback : callbacks) {
            if (callback) {
                callback(path, value);
            }
        }
    }

    void navigateUp() {
        std::vector<MenuItem> items = getCurrentMenuItems();
        if (items.empty()) return;

        currentSelection--;
        if (currentSelection < 0) {
            currentSelection = items.size() - 1;  // Wrap to end
        }
    }

    void navigateDown() {
        std::vector<MenuItem> items = getCurrentMenuItems();
        if (items.empty()) return;

        currentSelection++;
        if (currentSelection >= (int)items.size()) {
            currentSelection = 0;  // Wrap to start
        }
    }

    void selectCurrentItem() {
        std::vector<MenuItem> items = getCurrentMenuItems();
        if (items.empty() || currentSelection >= (int)items.size()) return;

        MenuItem item = items[currentSelection];

        switch (item.type) {
            case MenuItemType::SUBMENU:
                enterSubmenu(item.submenuPath);
                break;

            case MenuItemType::ACTION:
                executeAction(item.path);
                break;

            case MenuItemType::VALUE_EDIT:
                enterEditMode(item);
                break;

            case MenuItemType::TOGGLE:
                executeAction(item.path);
                break;
        }
    }

    void enterSubmenu(MenuPath submenu) {
        menuHistory.push_back(currentMenu);
        currentMenu = submenu;
        currentSelection = 0;
    }

    void navigateBack() {
        if (menuHistory.empty()) {
            return;  // Already at root
        }

        currentMenu = menuHistory.back();
        menuHistory.pop_back();
        currentSelection = 0;
    }

    void enterEditMode(MenuItem item) {
        inEditMode = true;
        editingItem = item;
        editValue = item.currentValue;
    }

    void confirmEdit() {
        inEditMode = false;

        // Update draft values based on what was edited
        switch (editingItem.path) {
            case MenuPath::CUSTOM_TEMP:
                customDraft.temp = editValue;
                break;
            case MenuPath::CUSTOM_TIME:
                customDraft.time = editValue * 60;  // Convert minutes to seconds
                break;
            case MenuPath::CUSTOM_OVERSHOOT:
                customDraft.overshoot = editValue;
                break;
            case MenuPath::ADJUST_TIMER:
                // Notify with the new value in minutes
                notifyCallbacks(editingItem.path, editValue);
                return;  // Don't call notifyCallbacks again below
            default:
                break;
        }

        // Notify callbacks with the new value
        notifyCallbacks(editingItem.path, editValue);
    }

    void cancelEdit() {
        inEditMode = false;
        // editValue discarded, draft values unchanged
    }

    void executeAction(MenuPath path) {
        // Notify with value 0 for actions
        notifyCallbacks(path, 0);

        // Some actions require navigation changes
        if (path == MenuPath::CUSTOM_SAVE || path == MenuPath::BACK) {
            navigateBack();
        }
    }

    void handleNavigationMode(MenuAction action) {
        switch (action) {
            case MenuAction::UP:
                navigateUp();
                break;

            case MenuAction::DOWN:
                navigateDown();
                break;

            case MenuAction::ENTER:
                selectCurrentItem();
                break;

            case MenuAction::BACK:
                navigateBack();
                break;
        }
    }

    void handleEditMode(MenuAction action) {
        switch (action) {
            case MenuAction::UP:
                editValue += editingItem.step;
                editValue = constrain(editValue, editingItem.minValue, editingItem.maxValue);
                break;

            case MenuAction::DOWN:
                editValue -= editingItem.step;
                editValue = constrain(editValue, editingItem.minValue, editingItem.maxValue);
                break;

            case MenuAction::ENTER:
                confirmEdit();
                break;

            case MenuAction::BACK:
                cancelEdit();
                break;
        }
    }

    // Round value to nearest multiple of step
    int roundToNearest(int value, int step) {
        return ((value + step / 2) / step) * step;
    }

    // Menu item generators
    std::vector<MenuItem> getRootMenu() {
        std::vector<MenuItem> items;

        MenuItem status;
        status.label = "Status";
        status.type = MenuItemType::SUBMENU;
        status.path = MenuPath::STATUS;
        status.submenuPath = MenuPath::STATUS;
        items.push_back(status);

        MenuItem preset;
        preset.label = "Select Preset";
        preset.type = MenuItemType::SUBMENU;
        preset.path = MenuPath::PRESET;
        preset.submenuPath = MenuPath::PRESET;
        items.push_back(preset);

        MenuItem editCustom;
        editCustom.label = "Edit Custom";
        editCustom.type = MenuItemType::SUBMENU;
        editCustom.path = MenuPath::PRESET_CUSTOM;
        editCustom.submenuPath = MenuPath::PRESET_CUSTOM;
        items.push_back(editCustom);

        MenuItem adjustTimer;
        adjustTimer.label = "Adjust Timer";
        adjustTimer.type = MenuItemType::VALUE_EDIT;
        adjustTimer.path = MenuPath::ADJUST_TIMER;
        // Round remaining time to nearest 10 minutes
        int remainingMinutes = currentRemainingTime / 60;
        adjustTimer.currentValue = roundToNearest(remainingMinutes, 10);
        adjustTimer.minValue = 10;  // 10 minutes minimum
        adjustTimer.maxValue = (int)(maxTime / 60);  // MAX_TIME_SECONDS in minutes
        adjustTimer.step = 10;
        adjustTimer.unit = "min";
        items.push_back(adjustTimer);

        MenuItem pid;
        pid.label = "PID: " + currentPIDProfile;
        pid.type = MenuItemType::SUBMENU;
        pid.path = MenuPath::PID_PROFILE;
        pid.submenuPath = MenuPath::PID_PROFILE;
        items.push_back(pid);

        MenuItem sound;
        sound.label = "Sound: " + String(soundEnabled ? "On" : "Off");
        sound.type = MenuItemType::TOGGLE;
        sound.path = soundEnabled ? MenuPath::SOUND_OFF : MenuPath::SOUND_ON;
        items.push_back(sound);

        MenuItem sysInfo;
        sysInfo.label = "System Info";
        sysInfo.type = MenuItemType::SUBMENU;
        sysInfo.path = MenuPath::SYSTEM_INFO;
        sysInfo.submenuPath = MenuPath::SYSTEM_INFO;
        items.push_back(sysInfo);

        return items;
    }

    std::vector<MenuItem> getStatusMenu() {
        std::vector<MenuItem> items;

        MenuItem start;
        start.label = "Start/Resume";
        start.type = MenuItemType::ACTION;
        start.path = MenuPath::STATUS_START;
        items.push_back(start);

        MenuItem pause;
        pause.label = "Pause";
        pause.type = MenuItemType::ACTION;
        pause.path = MenuPath::STATUS_PAUSE;
        items.push_back(pause);

        MenuItem reset;
        reset.label = "Ready";
        reset.type = MenuItemType::ACTION;
        reset.path = MenuPath::STATUS_RESET;
        items.push_back(reset);

        MenuItem back;
        back.label = "Back";
        back.type = MenuItemType::ACTION;
        back.path = MenuPath::BACK;
        items.push_back(back);

        return items;
    }

    std::vector<MenuItem> getPresetMenu() {
        std::vector<MenuItem> items;

        MenuItem pla;
        pla.label = "PLA";
        pla.type = MenuItemType::ACTION;
        pla.path = MenuPath::PRESET_PLA;
        items.push_back(pla);

        MenuItem petg;
        petg.label = "PETG";
        petg.type = MenuItemType::ACTION;
        petg.path = MenuPath::PRESET_PETG;
        items.push_back(petg);

        MenuItem custom;
        custom.label = "Custom";
        custom.type = MenuItemType::ACTION;
        custom.path = MenuPath::PRESET_CUSTOM;
        items.push_back(custom);

        MenuItem back;
        back.label = "Back";
        back.type = MenuItemType::ACTION;
        back.path = MenuPath::BACK;
        items.push_back(back);

        return items;
    }

    std::vector<MenuItem> getCustomPresetMenu() {
        std::vector<MenuItem> items;

        MenuItem temp;
        temp.label = "Temp";
        temp.type = MenuItemType::VALUE_EDIT;
        temp.path = MenuPath::CUSTOM_TEMP;
        temp.currentValue = (int)customDraft.temp;
        temp.minValue = (int)minTemp;
        temp.maxValue = (int)maxTemp;
        temp.step = 1;
        temp.unit = "C";
        items.push_back(temp);

        MenuItem time;
        time.label = "Time";
        time.type = MenuItemType::VALUE_EDIT;
        time.path = MenuPath::CUSTOM_TIME;
        time.currentValue = customDraft.time / 60;  // Display as minutes
        time.minValue = MIN_TIME_SECONDS / 60;
        time.maxValue = maxTime / 60;
        time.step = 10;
        time.unit = "min";
        items.push_back(time);

        MenuItem overshoot;
        overshoot.label = "Max Overshoot";
        overshoot.type = MenuItemType::VALUE_EDIT;
        overshoot.path = MenuPath::CUSTOM_OVERSHOOT;
        overshoot.currentValue = (int)customDraft.overshoot;
        overshoot.minValue = 0;
        overshoot.maxValue = (int)maxOvershoot;
        overshoot.step = 1;
        overshoot.unit = "C";
        items.push_back(overshoot);

        MenuItem copyPLA;
        copyPLA.label = "Copy from PLA";
        copyPLA.type = MenuItemType::ACTION;
        copyPLA.path = MenuPath::CUSTOM_COPY_PLA;
        items.push_back(copyPLA);

        MenuItem back;
        back.label = "Back";
        back.type = MenuItemType::ACTION;
        back.path = MenuPath::BACK;
        items.push_back(back);

        return items;
    }

    std::vector<MenuItem> getPIDProfileMenu() {
        std::vector<MenuItem> items;

        MenuItem soft;
        soft.label = "SOFT";
        soft.type = MenuItemType::ACTION;
        soft.path = MenuPath::PID_SOFT;
        items.push_back(soft);

        MenuItem normal;
        normal.label = "NORMAL";
        normal.type = MenuItemType::ACTION;
        normal.path = MenuPath::PID_NORMAL;
        items.push_back(normal);

        MenuItem strong;
        strong.label = "STRONG";
        strong.type = MenuItemType::ACTION;
        strong.path = MenuPath::PID_STRONG;
        items.push_back(strong);

        MenuItem back;
        back.label = "Back";
        back.type = MenuItemType::ACTION;
        back.path = MenuPath::BACK;
        items.push_back(back);

        return items;
    }

    std::vector<MenuItem> getSystemInfoMenu() {
        std::vector<MenuItem> items;

        // Create info items showing config values
        MenuItem stateSave;
        stateSave.label = "STATE_SAVE_INT";
        stateSave.type = MenuItemType::ACTION;
        stateSave.path = MenuPath::SYSTEM_INFO;
        stateSave.currentValue = STATE_SAVE_INTERVAL / 1000;
        stateSave.unit = "s";
        items.push_back(stateSave);

        MenuItem pidUpdate;
        pidUpdate.label = "PID_UPDATE_INT";
        pidUpdate.type = MenuItemType::ACTION;
        pidUpdate.path = MenuPath::SYSTEM_INFO;
        pidUpdate.currentValue = PID_UPDATE_INTERVAL;
        pidUpdate.unit = "ms";
        items.push_back(pidUpdate);

        MenuItem minTemp;
        minTemp.label = "MIN_TEMP";
        minTemp.type = MenuItemType::ACTION;
        minTemp.path = MenuPath::SYSTEM_INFO;
        minTemp.currentValue = (int)MIN_TEMP;
        minTemp.unit = "C";
        items.push_back(minTemp);

        MenuItem maxBoxTemp;
        maxBoxTemp.label = "MAX_BOX_TEMP";
        maxBoxTemp.type = MenuItemType::ACTION;
        maxBoxTemp.path = MenuPath::SYSTEM_INFO;
        maxBoxTemp.currentValue = (int)MAX_BOX_TEMP;
        maxBoxTemp.unit = "C";
        items.push_back(maxBoxTemp);

        MenuItem maxHeaterTemp;
        maxHeaterTemp.label = "MAX_HEATER_TEMP";
        maxHeaterTemp.type = MenuItemType::ACTION;
        maxHeaterTemp.path = MenuPath::SYSTEM_INFO;
        maxHeaterTemp.currentValue = (int)MAX_HEATER_TEMP;
        maxHeaterTemp.unit = "C";
        items.push_back(maxHeaterTemp);

        MenuItem defOvershoot;
        defOvershoot.label = "DEF_OVERSHOOT";
        defOvershoot.type = MenuItemType::ACTION;
        defOvershoot.path = MenuPath::SYSTEM_INFO;
        defOvershoot.currentValue = (int)DEFAULT_MAX_OVERSHOOT;
        defOvershoot.unit = "C";
        items.push_back(defOvershoot);

        MenuItem maxTime;
        maxTime.label = "MAX_TIME_SEC";
        maxTime.type = MenuItemType::ACTION;
        maxTime.path = MenuPath::SYSTEM_INFO;
        maxTime.currentValue = MAX_TIME_SECONDS;
        maxTime.unit = "s";
        items.push_back(maxTime);

        MenuItem minTime;
        minTime.label = "MIN_TIME_SEC";
        minTime.type = MenuItemType::ACTION;
        minTime.path = MenuPath::SYSTEM_INFO;
        minTime.currentValue = MIN_TIME_SECONDS;
        minTime.unit = "s";
        items.push_back(minTime);

        MenuItem pwmPeriod;
        pwmPeriod.label = "HEATER_PWM_PER";
        pwmPeriod.type = MenuItemType::ACTION;
        pwmPeriod.path = MenuPath::SYSTEM_INFO;
        pwmPeriod.currentValue = HEATER_PWM_PERIOD_MS;
        pwmPeriod.unit = "ms";
        items.push_back(pwmPeriod);

        MenuItem pwmMax;
        pwmMax.label = "PWM_MAX_OUTPUT";
        pwmMax.type = MenuItemType::ACTION;
        pwmMax.path = MenuPath::SYSTEM_INFO;
        pwmMax.currentValue = PWM_MAX_PID_OUTPUT;
        pwmMax.unit = "";
        items.push_back(pwmMax);

        MenuItem back;
        back.label = "Back";
        back.type = MenuItemType::ACTION;
        back.path = MenuPath::BACK;
        back.currentValue = 0;
        back.unit = "";
        items.push_back(back);

        return items;
    }

public:
    MenuController()
        : currentMenu(MenuPath::ROOT),
          currentSelection(0),
          inEditMode(false),
          minTemp(MIN_TEMP),
          maxTemp(MAX_BOX_TEMP),
          maxTime(MAX_TIME_SECONDS),
          maxOvershoot(DEFAULT_MAX_OVERSHOOT),
          currentPIDProfile("NORMAL"),
          soundEnabled(true),
          currentRemainingTime(14400) {  // Default 4 hours

        customDraft.temp = PRESET_CUSTOM_TEMP;
        customDraft.time = PRESET_CUSTOM_TIME;
        customDraft.overshoot = PRESET_CUSTOM_OVERSHOOT;
    }

    void handleAction(MenuAction action) override {
        if (inEditMode) {
            handleEditMode(action);
        } else {
            handleNavigationMode(action);
        }
    }

    std::vector<MenuItem> getCurrentMenuItems() override {
        switch (currentMenu) {
            case MenuPath::ROOT:
                return getRootMenu();
            case MenuPath::STATUS:
                return getStatusMenu();
            case MenuPath::PRESET:
                return getPresetMenu();
            case MenuPath::PRESET_CUSTOM:
                return getCustomPresetMenu();
            case MenuPath::PID_PROFILE:
                return getPIDProfileMenu();
            case MenuPath::SYSTEM_INFO:
                return getSystemInfoMenu();
            default:
                return getRootMenu();
        }
    }

    void setConstraints(float minT, float maxT, uint32_t maxTm, float maxOvr) override {
        minTemp = minT;
        maxTemp = maxT;
        maxTime = maxTm;
        maxOvershoot = maxOvr;
    }

    void setCustomPresetValues(float temp, uint32_t time, float overshoot) override {
        customDraft.temp = temp;
        customDraft.time = time;
        customDraft.overshoot = overshoot;
    }

    void setPIDProfile(const String& profile) {
        currentPIDProfile = profile;
    }

    void setSoundEnabled(bool enabled) {
        soundEnabled = enabled;
    }

    void setRemainingTime(uint32_t seconds) {
        currentRemainingTime = seconds;
    }

    void registerSelectionCallback(MenuSelectionCallback callback) override {
        callbacks.push_back(callback);
    }

    void reset() override {
        currentMenu = MenuPath::ROOT;
        currentSelection = 0;
        inEditMode = false;
        menuHistory.clear();
    }

    bool isInEditMode() const override {
        return inEditMode;
    }

    MenuItem getEditingItem() const {
        return editingItem;
    }

    int getEditValue() const {
        return editValue;
    }

    MenuPath getCurrentMenuPath() const {
        return currentMenu;
    }

    int getCurrentSelection() const {
        return currentSelection;
    }
};

#endif