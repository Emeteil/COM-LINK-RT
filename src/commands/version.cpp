#include "version.h"
#include <string.h>

namespace CommandVersion
{
    void Handler(const ComLinkRTProtocol::PacketHeader& header, const uint8_t* data)
    {
        if (header.packetId == ZERO_PACKET_ID)
            return;

        static_assert(sizeof(VersionInfo::buildDate) == sizeof(__DATE__), "buildDate size must match __DATE__");
        static_assert(sizeof(VersionInfo::buildTime) == sizeof(__TIME__), "buildTime size must match __TIME__");

        VersionInfo info;
        memset(&info, 0, sizeof(info));
        memcpy(info.buildDate, __DATE__, sizeof(info.buildDate));
        memcpy(info.buildTime, __TIME__, sizeof(info.buildTime));

        uint8_t txBuffer[64];
        uint16_t length;

        protocol.parser.CreatePacket(
            PROTOCOL_VERSION,
            PACKET_TYPE_RESPONSE,
            EMPTY_SERVICE_BITS,
            header.packetId,
            reinterpret_cast<uint8_t*>(&info), sizeof(info),
            txBuffer,
            length);

        protocol.SendPacket(txBuffer, length, header.packetId);
    }

    void Processor() {}
}
