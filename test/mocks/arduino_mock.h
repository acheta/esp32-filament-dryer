#pragma once

#ifdef UNIT_TEST

#ifndef ARDUINO_MOCK
#define ARDUINO_MOCK

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <ctime>
#include <string>

typedef uint8_t byte;
typedef bool boolean;

#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define DEVICE_DISCONNECTED_C -127.0f

// String class - same as before
class String {
private:
    std::string data;
public:
    String() : data("") {}
    String(const char* str) : data(str ? str : "") {}
    String(const std::string& str) : data(str) {}
    String(int val) : data(std::to_string(val)) {}
    String(float val) : data(std::to_string(val)) {}
    const char* c_str() const { return data.c_str(); }
    size_t length() const { return data.length(); }
    bool operator==(const String& other) const { return data == other.data; }
    bool operator!=(const String& other) const { return data != other.data; }
    String operator+(const String& other) const { return String(data + other.data); }
    int indexOf(const String& str) const {
        size_t pos = data.find(str.data);
        return (pos != std::string::npos) ? static_cast<int>(pos) : -1;
    }
};

// MockSerial - enhanced with all necessary overloads
class MockSerial {
public:
    void begin(unsigned long baud) { printf("Serial initialized at %lu baud\n", baud); }

    // print overloads
    void print(const char* str) { printf("%s", str); }
    void print(const String& str) { printf("%s", str.c_str()); }
    void print(int value) { printf("%d", value); }
    void print(unsigned int value) { printf("%u", value); }
    void print(long value) { printf("%ld", value); }
    void print(unsigned long value) { printf("%lu", value); }
    void print(float value, int decimals = 2) { printf("%.*f", decimals, value); }
    void print(double value, int decimals = 2) { printf("%.*f", decimals, value); }

    // println overloads
    void println(const char* str) { printf("%s\n", str); }
    void println(const String& str) { printf("%s\n", str.c_str()); }
    void println(int value) { printf("%d\n", value); }
    void println(unsigned int value) { printf("%u\n", value); }
    void println(long value) { printf("%ld\n", value); }
    void println(unsigned long value) { printf("%lu\n", value); }
    void println(float value, int decimals = 2) { printf("%.*f\n", decimals, value); }
    void println(double value, int decimals = 2) { printf("%.*f\n", decimals, value); }
    void println() { printf("\n"); }

    // available/read for serial input (stub implementations)
    int available() { return 0; }
    int read() { return -1; }
};

static MockSerial Serial;  // Changed to static

// ADD INLINE to all functions
inline unsigned long millis() {
    static unsigned long start_time = 0;
    if (start_time == 0) {
        start_time = clock() / (CLOCKS_PER_SEC / 1000);
    }
    return (clock() / (CLOCKS_PER_SEC / 1000)) - start_time;
}

inline void delay(unsigned long ms) {
    static unsigned long total_delay = 0;
    total_delay += ms;
}

inline void pinMode(uint8_t pin, uint8_t mode) {}
inline void digitalWrite(uint8_t pin, uint8_t value) {}
inline int digitalRead(uint8_t pin) { return LOW; }
inline int analogRead(uint8_t pin) { return 512; }
inline void analogWrite(uint8_t pin, int value) {}

// Math helpers
template<typename T>
inline T constrain(T x, T a, T b) {
    if (x < a) return a;
    if (x > b) return b;
    return x;
}

inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

#define F(string_literal) (string_literal)

// Include filesystem and JSON mocks after String class is defined
#include "MockFileSystem.h"
#include "MockArduinoJson.h"

#endif // ARDUINO_MOCK
#endif // UNIT_TEST