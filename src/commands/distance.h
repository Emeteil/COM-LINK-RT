#pragma once
#include "../protocol/com_protocol.h"

#define TRIG_PIN PC1
#define ECHO_PIN PC2

namespace CommandDistance
{   
    const uint8_t PACKET_TYPE_REQUEST = 0x05;
    const uint8_t PACKET_TYPE_RESPONSE = 0x06;
    const uint8_t PACKET_TYPE = PACKET_TYPE_REQUEST;

    void Handler(ComLinkRTProtocol::PacketHeader header, uint8_t* data);
    void Processor();
    void Init();
}