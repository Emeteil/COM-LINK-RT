#pragma once
#include <stddef.h>
#include <stdint.h>
#include <vector>
#include "mock_state.h"

namespace MockSpi
{
    struct State
    {
        bool begun;
        std::vector<uint8_t> written;
    };

    inline State& Get()
    {
        static State state;
        return state;
    }

    inline void Reset()
    {
        Get().begun = false;
        Get().written.clear();
    }

    inline const MockArduino::ResetHook resetHook(&Reset);
}

class SPISettings
{
    public:
    SPISettings() {}
    SPISettings(uint32_t, uint8_t, uint8_t) {}
};

class SPIClass
{
    public:
    void begin() { MockSpi::Get().begun = true; }
    void end() { MockSpi::Get().begun = false; }
    void beginTransaction(SPISettings) {}
    void endTransaction() {}
    void setBitOrder(uint8_t) {}
    void setDataMode(uint8_t) {}
    void setClockDivider(uint8_t) {}

    uint8_t transfer(uint8_t byte)
    {
        MockSpi::Get().written.push_back(byte);
        return 0;
    }

    void transfer(void* buffer, size_t size)
    {
        uint8_t* bytes = static_cast<uint8_t*>(buffer);
        for (size_t i = 0; i < size; i++)
            bytes[i] = transfer(bytes[i]);
    }
};

inline SPIClass SPI;
