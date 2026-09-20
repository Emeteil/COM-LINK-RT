#pragma once
#include <stddef.h>
#include <stdint.h>
#include <deque>
#include <vector>
#include "mock_state.h"

namespace MockWire
{
    struct Transmission
    {
        uint8_t address;
        std::vector<uint8_t> data;
        bool completed;
    };

    struct State
    {
        bool begun;
        uint16_t sda;
        uint16_t scl;
        uint32_t clockHz;
        std::vector<Transmission> transmissions;
        std::deque<uint8_t> readQueue;
    };

    inline State& Get()
    {
        static State state;
        return state;
    }

    inline void Reset()
    {
        State& state = Get();
        state.begun = false;
        state.sda = 0;
        state.scl = 0;
        state.clockHz = 0;
        state.transmissions.clear();
        state.readQueue.clear();
    }

    inline void QueueRead(const std::vector<uint8_t>& bytes)
    {
        Get().readQueue.insert(Get().readQueue.end(), bytes.begin(), bytes.end());
    }

    inline const MockArduino::ResetHook resetHook(&Reset);
}

class TwoWire
{
    public:
    void begin() { MockWire::Get().begun = true; }
    void begin(uint8_t) { MockWire::Get().begun = true; }
    void end() { MockWire::Get().begun = false; }
    void setSDA(uint16_t pin) { MockWire::Get().sda = pin; }
    void setSCL(uint16_t pin) { MockWire::Get().scl = pin; }
    void setClock(uint32_t hz) { MockWire::Get().clockHz = hz; }

    void beginTransmission(uint8_t address) { MockWire::Get().transmissions.push_back({address, {}, false}); }

    size_t write(uint8_t byte)
    {
        if (!MockWire::Get().transmissions.empty())
            MockWire::Get().transmissions.back().data.push_back(byte);
        return 1;
    }

    size_t write(const uint8_t* data, size_t size)
    {
        for (size_t i = 0; i < size; i++)
            write(data[i]);
        return size;
    }

    uint8_t endTransmission(bool = true)
    {
        if (!MockWire::Get().transmissions.empty())
            MockWire::Get().transmissions.back().completed = true;
        return 0;
    }

    uint8_t requestFrom(uint8_t, uint8_t count, bool = true) { return count; }

    int available() { return static_cast<int>(MockWire::Get().readQueue.size()); }

    int read()
    {
        if (MockWire::Get().readQueue.empty())
            return -1;

        uint8_t byte = MockWire::Get().readQueue.front();
        MockWire::Get().readQueue.pop_front();
        return byte;
    }
};

inline TwoWire Wire;
