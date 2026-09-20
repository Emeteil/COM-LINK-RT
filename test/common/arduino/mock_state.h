#pragma once
#include <stdint.h>
#include <map>
#include <vector>

namespace MockArduino
{
    struct PinEvent
    {
        uint16_t pin;
        int value;
        unsigned long timeMs;
    };

    inline std::vector<void (*)()>& ResetHooks()
    {
        static std::vector<void (*)()> hooks;
        return hooks;
    }

    struct ResetHook
    {
        explicit ResetHook(void (*fn)()) { ResetHooks().push_back(fn); }
    };

    struct State
    {
        unsigned long micros;
        std::vector<PinEvent> pinModes;
        std::vector<PinEvent> digitalWrites;
        std::vector<PinEvent> analogWrites;
        std::map<uint16_t, int> pinValues;
    };

    inline State& Get()
    {
        static State state;
        return state;
    }

    inline void ResetAll()
    {
        State& state = Get();
        state.micros = 0;
        state.pinModes.clear();
        state.digitalWrites.clear();
        state.analogWrites.clear();
        state.pinValues.clear();

        for (void (*hook)() : ResetHooks())
            hook();
    }

    inline unsigned long Millis()
    {
        return Get().micros / 1000UL;
    }

    inline unsigned long Micros()
    {
        return Get().micros;
    }

    inline void AdvanceMicros(unsigned long us)
    {
        Get().micros += us;
    }

    inline void Advance(unsigned long ms)
    {
        AdvanceMicros(ms * 1000UL);
    }

    inline void SetMillis(unsigned long ms)
    {
        Get().micros = ms * 1000UL;
    }

    inline void SetPinValue(uint16_t pin, int value)
    {
        Get().pinValues[pin] = value;
    }

    inline int PinValue(uint16_t pin)
    {
        std::map<uint16_t, int>& values = Get().pinValues;
        std::map<uint16_t, int>::const_iterator it = values.find(pin);
        return (it == values.end()) ? 0 : it->second;
    }

    inline int LastWrite(const std::vector<PinEvent>& log, uint16_t pin)
    {
        for (std::vector<PinEvent>::const_reverse_iterator it = log.rbegin(); it != log.rend(); ++it)
        {
            if (it->pin == pin)
                return it->value;
        }
        return -1;
    }

    inline int LastDigitalWrite(uint16_t pin)
    {
        return LastWrite(Get().digitalWrites, pin);
    }

    inline int LastAnalogWrite(uint16_t pin)
    {
        return LastWrite(Get().analogWrites, pin);
    }

    inline size_t DigitalWriteCount(uint16_t pin)
    {
        size_t count = 0;
        for (const PinEvent& event : Get().digitalWrites)
            if (event.pin == pin)
                count++;
        return count;
    }
}
