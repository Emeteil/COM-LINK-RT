#pragma once
#include "com_protocol_channel.h"

namespace ComLinkRTProtocol
{
    template <typename TSerial>
    class SerialChannel : public IProtocolChannel
    {
        private:
        TSerial& serial;
        unsigned long baudRate;

        public:
        SerialChannel(TSerial& serial, unsigned long baudRate) : serial(serial), baudRate(baudRate)
        {
        }

        void Begin() override
        {
            serial.begin(baudRate);
        }

        int Available() override
        {
            return serial.available();
        }

        uint8_t Read() override
        {
            return static_cast<uint8_t>(serial.read());
        }

        void Write(const uint8_t* data, uint16_t length) override
        {
            serial.write(data, length);
        }
    };
}
