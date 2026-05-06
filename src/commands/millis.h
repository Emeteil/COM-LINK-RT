#pragma once
#include "../protocol/com_protocol.h"

namespace CommandMillis
{
    const uint8_t PACKET_TYPE_REQUEST = 0x03;
    const uint8_t PACKET_TYPE_RESPONSE = 0x04;
    const uint8_t PACKET_TYPE = PACKET_TYPE_REQUEST;
        
    void Handler(const ComLinkRTProtocol::PacketHeader& header, const uint8_t* data);
    void Processor();
}