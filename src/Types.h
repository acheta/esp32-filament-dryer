#ifndef TYPES_H
#define TYPES_H

#include <functional>
#ifndef UNIT_TEST
    #include <Arduino.h>
#else
    // Mock Arduino functions for native testing
    #include "../test/mocks/arduino_mock.h"
#endif

// ==================== Enums ====================

enum class DryerState {
    READY,
    RUNNING,
    PAUSED,
    FINISHED,
    FAILED,
    POWER_RECOVERED
};

enum class PresetType {
    PLA,
    PETG,
    CUSTOM
};

enum class PIDProfile {
    SOFT,    // Kp=2.0, Ki=0.5, Kd=1.0
    NORMAL,  // Kp=4.0, Ki=1.0, Kd=2.0
    STRONG   // Kp=6.0, Ki=1.5, Kd=3.0
};

enum class SensorType {
    HEATER_TEMP,
    BOX_TEMP,
    BOX_HUMIDITY
};

enum class MenuAction {
    UP,
    DOWN,
    ENTER,
    BACK
};

enum class MenuPath {
    ROOT,
    STATUS,
    STATUS_START,
    STATUS_PAUSE,
    STATUS_RESET,
    PRESET,
    PRESET_PLA,
    PRESET_PETG,
    PRESET_CUSTOM,
    CUSTOM_TEMP,
    CUSTOM_TIME,
    CUSTOM_OVERSHOOT,
    CUSTOM_SAVE,
    CUSTOM_COPY_PLA,
    CUSTOM_BACK,
    PID_PROFILE,
    PID_SOFT,
    PID_NORMAL,
    PID_STRONG,
    SOUND,
    SOUND_ON,
    SOUND_OFF,
    SYSTEM_INFO,
    ADJUST_TIMER,
    BACK
};

enum class MenuItemType {
    SUBMENU,
    ACTION,
    VALUE_EDIT,
    TOGGLE
};

enum class ButtonType {
    SET,
    UP,
    DOWN
};

enum class ButtonEvent {
    SINGLE_CLICK,
    LONG_PRESS
};

// ==================== Structs ====================

struct DryingPreset {
    float targetTemp;      // °C
    uint32_t targetTime;   // seconds
    float maxOvershoot;    // °C above target

    DryingPreset() : targetTemp(50.0), targetTime(14400), maxOvershoot(10.0) {}
    DryingPreset(float temp, uint32_t time, float overshoot)
        : targetTemp(temp), targetTime(time), maxOvershoot(overshoot) {}
};

struct SensorReading {
    float value;
    uint32_t timestamp;
    bool isValid;

    SensorReading() : value(0.0), timestamp(0), isValid(false) {}
    SensorReading(float v, uint32_t t, bool valid)
        : value(v), timestamp(t), isValid(valid) {}
};

struct SensorReadings {
    SensorReading heaterTemp;
    SensorReading boxTemp;
    SensorReading boxHumidity;

    SensorReadings() = default;
};

struct CurrentStats {
    DryerState state;
    float currentTemp;
    float targetTemp;
    float boxTemp;
    float boxHumidity;
    uint32_t elapsedTime;
    uint32_t remainingTime;
    float pwmOutput;
    PresetType activePreset;

    CurrentStats() : state(DryerState::READY), currentTemp(0), targetTemp(0),
                     boxTemp(0), boxHumidity(0), elapsedTime(0),
                     remainingTime(0), pwmOutput(0), activePreset(PresetType::PLA) {}
};

struct MenuItem {
    String label;
    MenuItemType type;
    MenuPath path;
    int currentValue;
    int minValue;
    int maxValue;
    int step;
    String unit;
    MenuPath submenuPath;

    MenuItem() : type(MenuItemType::ACTION), path(MenuPath::ROOT),
                 currentValue(0), minValue(0), maxValue(0), step(1),
                 submenuPath(MenuPath::ROOT) {}

    MenuItem(String lbl, MenuItemType t, MenuPath p)
        : label(lbl), type(t), path(p), currentValue(0), minValue(0),
          maxValue(0), step(1), submenuPath(MenuPath::ROOT) {}
};

// ==================== Callback Types ====================

// Sensor callbacks
using HeaterTempCallback = std::function<void(float temp, uint32_t timestamp)>;
using BoxDataCallback = std::function<void(float temp, float humidity, uint32_t timestamp)>;
using SensorErrorCallback = std::function<void(SensorType type, const String& error)>;

// Safety callbacks
using EmergencyStopCallback = std::function<void(const String& reason)>;

// State callbacks
using StateChangeCallback = std::function<void(DryerState oldState, DryerState newState)>;
using StatsUpdateCallback = std::function<void(const CurrentStats& stats)>;

// Menu callbacks
using MenuSelectionCallback = std::function<void(MenuPath path, int value)>;

// Button callbacks
using ButtonCallback = std::function<void(ButtonEvent event)>;

#endif