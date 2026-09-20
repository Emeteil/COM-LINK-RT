#pragma once
#include <stddef.h>
#include <stdint.h>
#include <deque>
#include <string>
#include <vector>
#include "mock_state.h"

namespace arduino
{
    class Print
    {
        public:
        virtual ~Print() = default;
        virtual size_t write(uint8_t byte) = 0;

        virtual size_t write(const uint8_t* buffer, size_t size)
        {
            size_t written = 0;
            for (size_t i = 0; i < size; i++)
                written += write(buffer[i]);
            return written;
        }

        size_t write(const char* text)
        {
            size_t written = 0;
            while (text != nullptr && *text != '\0')
                written += write(static_cast<uint8_t>(*text++));
            return written;
        }

        size_t print(const char* text) { return write(text); }
        size_t print(char value) { return write(static_cast<uint8_t>(value)); }
        size_t print(int value) { return write(std::to_string(value).c_str()); }
        size_t print(unsigned int value) { return write(std::to_string(value).c_str()); }
        size_t print(long value) { return write(std::to_string(value).c_str()); }
        size_t print(unsigned long value) { return write(std::to_string(value).c_str()); }
        size_t print(double value) { return write(std::to_string(value).c_str()); }

        template <typename T>
        size_t println(const T& value)
        {
            return print(value) + write(static_cast<uint8_t>('\n'));
        }

        size_t println() { return write(static_cast<uint8_t>('\n')); }

        virtual void flush() {}
    };

    class Stream : public Print
    {
        public:
        virtual int available() = 0;
        virtual int read() = 0;
        virtual int peek() = 0;
    };

    class HardwareSerial;

    inline std::vector<HardwareSerial*>& SerialRegistry()
    {
        static std::vector<HardwareSerial*> ports;
        return ports;
    }

    class HardwareSerial : public Stream
    {
        public:
        std::deque<uint8_t> rx;
        std::vector<uint8_t> tx;
        unsigned long baudRate;
        bool started;

        HardwareSerial() : baudRate(0), started(false) { SerialRegistry().push_back(this); }

        void begin(unsigned long baud)
        {
            baudRate = baud;
            started = true;
        }

        void begin(unsigned long baud, uint8_t) { begin(baud); }

        void end() { started = false; }

        operator bool() const { return started; }

        int available() override { return static_cast<int>(rx.size()); }

        int availableForWrite() { return 64; }

        int read() override
        {
            if (rx.empty())
                return -1;

            uint8_t byte = rx.front();
            rx.pop_front();
            return byte;
        }

        int peek() override { return rx.empty() ? -1 : rx.front(); }

        size_t write(uint8_t byte) override
        {
            tx.push_back(byte);
            return 1;
        }

        size_t write(const uint8_t* buffer, size_t size) override
        {
            tx.insert(tx.end(), buffer, buffer + size);
            return size;
        }

        void Feed(const uint8_t* data, size_t size) { rx.insert(rx.end(), data, data + size); }

        void Feed(const std::vector<uint8_t>& data) { Feed(data.data(), data.size()); }

        void Clear()
        {
            rx.clear();
            tx.clear();
        }
    };

    inline void ResetSerialPorts()
    {
        for (HardwareSerial* port : SerialRegistry())
            port->Clear();
    }

    inline const MockArduino::ResetHook serialResetHook(&ResetSerialPorts);

    inline HardwareSerial Serial;
    inline HardwareSerial Serial1;
    inline HardwareSerial Serial2;
    inline HardwareSerial Serial3;
    inline HardwareSerial Serial4;
    inline HardwareSerial Serial5;
    inline HardwareSerial Serial6;
}

using arduino::HardwareSerial;
using arduino::Print;
using arduino::Serial;
using arduino::Serial1;
using arduino::Serial2;
using arduino::Serial3;
using arduino::Serial4;
using arduino::Serial5;
using arduino::Serial6;
using arduino::Stream;
