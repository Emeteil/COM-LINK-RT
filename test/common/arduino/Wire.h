#pragma once
#include <stddef.h>
#include <stdint.h>
#include <deque>
#include <map>
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

    struct Device
    {
        std::map<uint8_t, uint8_t> registers;
        uint8_t pointer;
    };

    struct State
    {
        bool begun;
        uint16_t sda;
        uint16_t scl;
        uint32_t clockHz;
        std::vector<Transmission> transmissions;
        std::map<uint8_t, Device> devices;
        std::deque<uint8_t> readQueue;
        size_t reads;
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
        state.devices.clear();
        state.readQueue.clear();
        state.reads = 0;
    }

    inline void AddDevice(uint8_t address) { Get().devices[address] = Device{{}, 0}; }

    inline void RemoveDevice(uint8_t address) { Get().devices.erase(address); }

    inline void SetRegister(uint8_t address, uint8_t reg, uint8_t value) { Get().devices[address].registers[reg] = value; }

    inline uint8_t Register(uint8_t address, uint8_t reg) { return Get().devices[address].registers[reg]; }

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
        MockWire::State& state = MockWire::Get();
        if (state.transmissions.empty())
            return 4;

        MockWire::Transmission& tx = state.transmissions.back();
        tx.completed = true;

        std::map<uint8_t, MockWire::Device>::iterator it = state.devices.find(tx.address);
        if (it == state.devices.end())
            return 2;

        if (!tx.data.empty())
        {
            it->second.pointer = tx.data[0];
            for (size_t i = 1; i < tx.data.size(); i++)
                it->second.registers[static_cast<uint8_t>(tx.data[0] + i - 1)] = tx.data[i];
        }
        return 0;
    }

    uint8_t requestFrom(uint8_t address, uint8_t count, bool = true)
    {
        MockWire::State& state = MockWire::Get();
        std::map<uint8_t, MockWire::Device>::iterator it = state.devices.find(address);
        if (it == state.devices.end())
            return 0;

        state.reads++;
        for (uint8_t i = 0; i < count; i++)
            state.readQueue.push_back(it->second.registers[static_cast<uint8_t>(it->second.pointer + i)]);
        it->second.pointer = static_cast<uint8_t>(it->second.pointer + count);
        return count;
    }

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
