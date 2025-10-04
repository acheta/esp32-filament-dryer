#ifndef I_DISPLAY_H
#define I_DISPLAY_H

#include "../Types.h"

/**
 * Interface for Display
 *
 * Responsibilities:
 * - Render sensor readings on 128x32 OLED
 * - Clear and refresh display
 * - Low-level text rendering
 *
 * Does NOT:
 * - Decide what to display (UI controller does this)
 * - Read sensors directly
 * - Handle user input
 * - Format complex layouts (just sensor readings for now)
 */
class IDisplay {
public:
    virtual ~IDisplay() = default;

    virtual void begin() = 0;
    virtual void clear() = 0;
    virtual void display() = 0;

    // Show sensor readings (main display mode)
    virtual void showSensorReadings(float heaterTemp, bool heaterValid,
                                   float boxTemp, float boxHumidity, bool boxValid) = 0;

    // Low-level drawing primitives
    virtual void setCursor(uint8_t x, uint8_t y) = 0;
    virtual void setTextSize(uint8_t size) = 0;
    virtual void print(const String& text) = 0;
    virtual void println(const String& text) = 0;
};

#endif