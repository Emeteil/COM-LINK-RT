#pragma once
#include "com_protocol_channel.h"

namespace arduino
{
    class HardwareSerial;
}
using arduino::HardwareSerial;

namespace ComLinkRTProtocol
{
    class SerialChannel : public IProtocolChannel
    {
        private:
        HardwareSerial& serial;
        unsigned long baudRate;

        public:
        SerialChannel(HardwareSerial& serial, unsigned long baudRate);

        void Begin() override;
        int Available() override;
        uint8_t Read() override;
        void Write(const uint8_t* data, uint16_t length) override;
    };
}
