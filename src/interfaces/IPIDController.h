#ifndef I_PID_CONTROLLER_H
#define I_PID_CONTROLLER_H

#include "../Types.h"

class IPIDController {
public:
    virtual ~IPIDController() = default;
    virtual void begin() = 0;
    virtual void setProfile(PIDProfile profile) = 0;
    virtual void setLimits(float outMin, float outMax) = 0;
    virtual void setMaxAllowedTemp(float maxTemp) = 0;
    virtual float compute(float setpoint, float input, uint32_t currentMillis) = 0;
    virtual void reset() = 0;
};

#endif