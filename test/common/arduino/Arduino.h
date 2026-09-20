#pragma once
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include "HardwareSerial.h"
#include "mock_state.h"

#define HIGH 0x1
#define LOW  0x0

#define INPUT          0x0
#define OUTPUT         0x1
#define INPUT_PULLUP   0x2
#define INPUT_PULLDOWN 0x3

#define LSBFIRST 0
#define MSBFIRST 1

#define CHANGE  1
#define FALLING 2
#define RISING  3

#define PI         3.1415926535897932384626433832795
#define HALF_PI    1.5707963267948966192313216916398
#define TWO_PI     6.283185307179586476925286766559
#define DEG_TO_RAD 0.017453292519943295769236907684886
#define RAD_TO_DEG 57.295779513082320876798154814105

#define PROGMEM
#define PSTR(s) (s)
#define F(s)    (s)

#define pgm_read_byte(addr)      (*reinterpret_cast<const uint8_t*>(addr))
#define pgm_read_word(addr)      (*reinterpret_cast<const uint16_t*>(addr))
#define pgm_read_dword(addr)     (*reinterpret_cast<const uint32_t*>(addr))
#define pgm_read_ptr(addr)       (*reinterpret_cast<void* const*>(addr))
#define strlen_P(s)              strlen(s)
#define strcpy_P(dst, src)       strcpy(dst, src)
#define memcpy_P(dst, src, size) memcpy(dst, src, size)

typedef uint8_t byte;
typedef bool boolean;
typedef unsigned int word;

#define MOCK_PORT_PINS(port, base) port##0 = base, port##1, port##2, port##3, port##4, port##5, port##6, port##7, port##8, port##9, port##10, port##11, port##12, port##13, port##14, port##15

enum PinName : uint16_t
{
    MOCK_PORT_PINS(PA, 0),
    MOCK_PORT_PINS(PB, 16),
    MOCK_PORT_PINS(PC, 32),
    MOCK_PORT_PINS(PD, 48),
    MOCK_PORT_PINS(PE, 64),
    MOCK_PORT_PINS(PF, 80),
    MOCK_PORT_PINS(PG, 96),
    MOCK_PORT_PINS(PH, 112),
    NC = 0xFFFF
};

#define LED_BUILTIN PA5

inline void pinMode(uint16_t pin, uint8_t mode)
{
    MockArduino::Get().pinModes.push_back({pin, static_cast<int>(mode), MockArduino::Millis()});
}

inline void digitalWrite(uint16_t pin, uint8_t value)
{
    MockArduino::Get().digitalWrites.push_back({pin, static_cast<int>(value), MockArduino::Millis()});
    MockArduino::Get().pinValues[pin] = value;
}

inline int digitalRead(uint16_t pin)
{
    return MockArduino::PinValue(pin);
}

inline void analogWrite(uint16_t pin, int value)
{
    MockArduino::Get().analogWrites.push_back({pin, value, MockArduino::Millis()});
    MockArduino::Get().pinValues[pin] = value;
}

inline int analogRead(uint16_t pin)
{
    return MockArduino::PinValue(pin);
}

inline void analogReadResolution(int) {}
inline void analogWriteResolution(int) {}
inline void analogWriteFrequency(uint32_t) {}

inline unsigned long millis()
{
    return MockArduino::Millis();
}

inline unsigned long micros()
{
    return MockArduino::Micros();
}

inline void delay(unsigned long ms)
{
    MockArduino::Advance(ms);
}

inline void delayMicroseconds(unsigned int us)
{
    MockArduino::AdvanceMicros(us);
}

inline void yield() {}
inline void interrupts() {}
inline void noInterrupts() {}
inline uint16_t digitalPinToInterrupt(uint16_t pin) { return pin; }
inline void attachInterrupt(uint16_t, void (*)(), int) {}
inline void detachInterrupt(uint16_t) {}
inline void tone(uint16_t, unsigned int, unsigned long = 0) {}
inline void noTone(uint16_t) {}
inline unsigned long pulseIn(uint16_t, uint8_t, unsigned long = 1000000UL) { return 0; }
inline void shiftOut(uint16_t, uint16_t, uint8_t, uint8_t) {}
inline uint8_t shiftIn(uint16_t, uint16_t, uint8_t) { return 0; }

inline long random(long max)
{
    return (max <= 0) ? 0 : (rand() % max);
}

inline long random(long min, long max)
{
    return (max <= min) ? min : (min + rand() % (max - min));
}

inline void randomSeed(unsigned long seed)
{
    srand(static_cast<unsigned int>(seed));
}

inline long map(long value, long inMin, long inMax, long outMin, long outMax)
{
    if (inMax == inMin)
        return outMin;
    return (value - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
}

template <typename T, typename U>
inline auto min(T a, U b) -> decltype((a < b) ? a : b)
{
    return (a < b) ? a : b;
}

template <typename T, typename U>
inline auto max(T a, U b) -> decltype((a > b) ? a : b)
{
    return (a > b) ? a : b;
}

template <typename T, typename L, typename H>
inline T constrain(T value, L low, H high)
{
    if (value < static_cast<T>(low))
        return static_cast<T>(low);
    if (value > static_cast<T>(high))
        return static_cast<T>(high);
    return value;
}

class String
{
    public:
    String() {}
    String(const char* text) : value(text != nullptr ? text : "") {}
    String(const std::string& text) : value(text) {}
    String(int number) : value(std::to_string(number)) {}
    String(long number) : value(std::to_string(number)) {}
    String(unsigned long number) : value(std::to_string(number)) {}
    String(double number) : value(std::to_string(number)) {}

    const char* c_str() const { return value.c_str(); }
    unsigned int length() const { return static_cast<unsigned int>(value.size()); }
    char charAt(unsigned int index) const { return index < value.size() ? value[index] : '\0'; }
    int toInt() const { return atoi(value.c_str()); }
    double toDouble() const { return atof(value.c_str()); }

    String& operator+=(const String& other)
    {
        value += other.value;
        return *this;
    }

    String operator+(const String& other) const { return String(value + other.value); }
    bool operator==(const String& other) const { return value == other.value; }
    bool operator!=(const String& other) const { return value != other.value; }
    char operator[](unsigned int index) const { return charAt(index); }

    std::string value;
};
