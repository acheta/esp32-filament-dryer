#ifndef I_MENU_CONTROLLER_H
#define I_MENU_CONTROLLER_H

#include "../Types.h"
#include <vector>

class IMenuController {
public:
    virtual ~IMenuController() = default;
    virtual void handleAction(MenuAction action) = 0;
    virtual std::vector<MenuItem> getCurrentMenuItems() = 0;
    virtual void setConstraints(float minTemp, float maxTemp,
                                uint32_t maxTime, float maxOvershoot) = 0;
    virtual void setCustomPresetValues(float temp, uint32_t time, float overshoot) = 0;
    virtual void registerSelectionCallback(MenuSelectionCallback callback) = 0;
    virtual void reset() = 0;
    virtual bool isInEditMode() const = 0;
};

#endif