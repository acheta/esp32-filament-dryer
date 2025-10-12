#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include "../interfaces/IDisplay.h"
#include "../Config.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

/**
 * OLEDDisplay - 128x32 SSD1306 OLED implementation
 *
 * Hardware: I2C OLED display, typically at address 0x3C
 */
class OLEDDisplay : public IDisplay {
private:
    Adafruit_SSD1306 oledDisplay;
    uint8_t i2cAddress;

public:
    OLEDDisplay(uint8_t width = 128, uint8_t height = 32, uint8_t address = 0x3C)
        : oledDisplay(width, height, &Wire, -1),
          i2cAddress(address) {
    }

    void begin() override {
        if (!oledDisplay.begin(SSD1306_SWITCHCAPVCC, i2cAddress)) {
            // Display initialization failed - could notify via error callback
            return;
        }

        oledDisplay.clearDisplay();
        oledDisplay.setTextColor(SSD1306_WHITE);
        oledDisplay.setTextSize(1);
        oledDisplay.setTextWrap(false);  // Disable text wrapping
        oledDisplay.display();
    }

    void clear() override {
        oledDisplay.clearDisplay();
    }

    void display() override {
        oledDisplay.display();
    }

    void showSensorReadings(float heaterTemp, bool heaterValid,
                           float boxTemp, float boxHumidity, bool boxValid) override {
        clear();

        // Line 1: Heater temperature (larger text)
        oledDisplay.setTextSize(1);
        oledDisplay.setCursor(0, 0);
        oledDisplay.print("Heater: ");

        if (heaterValid) {
            oledDisplay.setTextSize(2);
            oledDisplay.print(heaterTemp, 1);
            oledDisplay.setTextSize(1);
            oledDisplay.print("C");
        } else {
            oledDisplay.setTextSize(1);
            oledDisplay.print("--");
        }

        // Line 2: Box temperature
        oledDisplay.setTextSize(1);
        oledDisplay.setCursor(0, 16);
        oledDisplay.print("Box: ");

        if (boxValid) {
            oledDisplay.print(boxTemp, 1);
            oledDisplay.print("C ");
            oledDisplay.print(boxHumidity, 0);
            oledDisplay.print("%");
        } else {
            oledDisplay.print("--");
        }

        oledDisplay.display();
    }

    void setCursor(uint8_t x, uint8_t y) override {
        oledDisplay.setCursor(x, y);
    }

    void setTextSize(uint8_t size) override {
        oledDisplay.setTextSize(size);
    }

    void print(const String& text) override {
        oledDisplay.print(text.c_str());
    }

    void println(const String& text) override {
        oledDisplay.println(text.c_str());
    }
};

#endif