#ifndef I_FAN_CONTROL_H
#define I_FAN_CONTROL_H

/**
 * Interface for Fan Control
 *
 * Responsibilities:
 * - Control fan relay (ON/OFF)
 * - Report fan running state
 *
 * Does NOT:
 * - Make decisions about when to run (Dryer does this)
 * - Monitor temperature or other conditions
 */
class IFanControl {
public:
    virtual ~IFanControl() = default;

    virtual void start() = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
};

#endif