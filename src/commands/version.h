#pragma once
#include "../core/globals.h"

namespace CommandVersion
{
    const uint8_t PACKET_TYPE_REQUEST = 0x05;
    const uint8_t PACKET_TYPE_RESPONSE = 0x06;
    const uint8_t PACKET_TYPE = PACKET_TYPE_REQUEST;

#pragma pack(push, 1)
    struct VersionInfo
    {
        char buildDate[12];
        char buildTime[9];
    };
#pragma pack(pop)

    void Handler(const ComLinkRTProtocol::PacketHeader& header, const uint8_t* data);
    void Processor();
}
