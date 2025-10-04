#ifndef I_SOUND_CONTROLLER_H
#define I_SOUND_CONTROLLER_H

class ISoundController {
public:
    virtual ~ISoundController() = default;
    virtual void begin() = 0;
    virtual void setEnabled(bool enabled) = 0;
    virtual bool isEnabled() const = 0;
    virtual void playClick() = 0;
    virtual void playConfirm() = 0;
    virtual void playStart() = 0;
    virtual void playFinished() = 0;
    virtual void playAlarm() = 0;
};

#endif