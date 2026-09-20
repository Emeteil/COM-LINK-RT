#pragma once
#include <stdint.h>
#include <map>
#include <vector>
#include "Wire.h"
#include "mock_state.h"

namespace MockPwm
{
    struct PwmCall
    {
        uint8_t channel;
        uint16_t on;
        uint16_t off;
        unsigned long timeMs;
    };

    struct State
    {
        bool beginResult;
        bool begun;
        uint8_t address;
        float frequencyHz;
        std::vector<PwmCall> calls;
        std::map<uint8_t, PwmCall> channels;
    };

    inline State& Get()
    {
        static State state;
        return state;
    }

    inline void Reset()
    {
        State& state = Get();
        state.beginResult = true;
        state.begun = false;
        state.address = 0;
        state.frequencyHz = 0.0f;
        state.calls.clear();
        state.channels.clear();
    }

    inline bool HasChannel(uint8_t channel)
    {
        return Get().channels.find(channel) != Get().channels.end();
    }

    inline uint16_t ChannelOff(uint8_t channel)
    {
        std::map<uint8_t, PwmCall>::const_iterator it = Get().channels.find(channel);
        return (it == Get().channels.end()) ? 0 : it->second.off;
    }

    inline size_t CallCount()
    {
        return Get().calls.size();
    }

    inline const MockArduino::ResetHook resetHook(&Reset);
}

class Adafruit_PWMServoDriver
{
    public:
    Adafruit_PWMServoDriver() : address(0x40) {}
    explicit Adafruit_PWMServoDriver(const uint8_t addr) : address(addr) {}
    Adafruit_PWMServoDriver(const uint8_t addr, TwoWire&) : address(addr) {}

    bool begin(uint8_t = 0)
    {
        MockPwm::Get().address = address;
        MockPwm::Get().begun = MockPwm::Get().beginResult;
        return MockPwm::Get().beginResult;
    }

    void reset() {}
    void sleep() {}
    void wakeup() {}
    void setExtClk(uint8_t) {}
    void setOscillatorFrequency(uint32_t) {}
    uint32_t getOscillatorFrequency() { return 25000000; }

    void setPWMFreq(float frequency) { MockPwm::Get().frequencyHz = frequency; }

    void setPWM(uint8_t channel, uint16_t on, uint16_t off)
    {
        MockPwm::PwmCall call = {channel, on, off, MockArduino::Millis()};
        MockPwm::Get().calls.push_back(call);
        MockPwm::Get().channels[channel] = call;
    }

    void setPin(uint8_t channel, uint16_t value, bool = false) { setPWM(channel, 0, value); }

    uint8_t getPWM(uint8_t channel, bool = false) { return static_cast<uint8_t>(MockPwm::ChannelOff(channel)); }

    void writeMicroseconds(uint8_t channel, uint16_t microseconds) { setPWM(channel, 0, microseconds); }

    private:
    uint8_t address;
};
