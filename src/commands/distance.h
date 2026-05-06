#pragma once
#include "../protocol/com_protocol.h"
#include "../core/board_config.h"

#define TRIG_PIN HC_SR04_TRIG_PIN
#define ECHO_PIN HC_SR04_ECHO_PIN

namespace CommandDistance
{   
    const uint8_t PACKET_TYPE_REQUEST = 0x05;
    const uint8_t PACKET_TYPE_RESPONSE = 0x06;
    const uint8_t PACKET_TYPE = PACKET_TYPE_REQUEST;

    void Handler(const ComLinkRTProtocol::PacketHeader& header, const uint8_t* data);
    void Processor();
    void Init();
}