#ifndef I_SAFETY_MONITOR_H
#define I_SAFETY_MONITOR_H

#include "../Types.h"

class ISafetyMonitor {
public:
    virtual ~ISafetyMonitor() = default;
    virtual void begin() = 0;
    virtual void update(uint32_t currentMillis) = 0;
    virtual void setMaxHeaterTemp(float temp) = 0;
    virtual void setMaxBoxTemp(float temp) = 0;
    virtual void notifyHeaterTemp(float temp, uint32_t timestamp) = 0;
    virtual void notifyBoxTemp(float temp, uint32_t timestamp) = 0;
    virtual void registerEmergencyStopCallback(EmergencyStopCallback callback) = 0;
};

#endif
