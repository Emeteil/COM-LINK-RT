#pragma once
#include "../core/globals.h"

namespace CommandPing
{
    const uint8_t PACKET_TYPE_REQUEST = 0x01;
    const uint8_t PACKET_TYPE_RESPONSE = 0x02;
    const uint8_t PACKET_TYPE = PACKET_TYPE_REQUEST;

    void Handler(const ComLinkRTProtocol::PacketHeader& header, const uint8_t* data);
    void Processor();
}