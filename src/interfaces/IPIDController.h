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

    /**
     * Compute PID output based on box temperature (primary control) and heater temperature (constraint)
     * @param setpoint Target box temperature
     * @param boxTemp Current box temperature (process variable for PID)
     * @param heaterTemp Current heater temperature (used for dynamic limiting)
     * @param currentMillis Current time in milliseconds
     * @return PWM output (0-PWM_MAX_PID_OUTPUT)
     */
    virtual float compute(float setpoint, float boxTemp, float heaterTemp, uint32_t currentMillis) = 0;

    virtual void reset() = 0;
};

#endif