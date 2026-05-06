#include "ping.h"

namespace CommandPing
{
    void Handler(const ComLinkRTProtocol::PacketHeader& header, const uint8_t* data)
    {
        if (header.packetId == ZERO_PACKET_ID)
            return;

        uint8_t txBuffer[64];
        uint16_t length;

        protocol.parser.CreatePacket(
            PROTOCOL_VERSION,
            PACKET_TYPE_RESPONSE,
            EMPTY_SERVICE_BITS,
            header.packetId,
            nullptr, 0,
            txBuffer,
            length
        );

        protocol.SendPacket(txBuffer, length, header.packetId);
    }

    void Processor() {}
}