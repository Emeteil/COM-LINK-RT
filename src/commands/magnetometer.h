#pragma once
#include "../protocol/com_protocol.h"
#include "../core/board_config.h"

namespace CommandMagnetometer
{
    const uint8_t PACKET_TYPE_REQUEST = 0x0B;
    const uint8_t PACKET_TYPE_RESPONSE = 0x0C;
    const uint8_t PACKET_TYPE = PACKET_TYPE_REQUEST;

    enum Status : uint8_t
    {
        STATUS_OK = 0x00,
        STATUS_NOT_FOUND = 0x01,
        STATUS_OVERFLOW = 0x02,
        STATUS_NO_DATA = 0x03,
        STATUS_BUS_STUCK = 0x04
    };

#pragma pack(push, 1)
    struct MagnetometerData
    {
        int16_t x;
        int16_t y;
        int16_t z;
        uint8_t status;
    };
#pragma pack(pop)

    void Init();
    void Reset();
    void Handler(const ComLinkRTProtocol::PacketHeader& header, const uint8_t* data);
    void Processor();
}
