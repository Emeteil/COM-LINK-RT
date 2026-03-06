#include "ping.h"

namespace CommandPing
{
    void Handler(ComLinkRTProtocol::PacketHeader header, uint8_t* data)
    {
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