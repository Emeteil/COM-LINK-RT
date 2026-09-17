#pragma once
#include <stdint.h>

namespace ComLinkRTProtocol
{
    class IProtocolChannel
    {
        public:
        virtual ~IProtocolChannel() = default;

        virtual void Begin() = 0;
        virtual int Available() = 0;
        virtual uint8_t Read() = 0;
        virtual void Write(const uint8_t* data, uint16_t length) = 0;
    };
}
