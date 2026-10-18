#pragma once
#include "../protocol/com_protocol.h"
#include "../core/board_config.h"

namespace CommandDistance
{
    const uint8_t PACKET_TYPE_REQUEST = 0x09;
    const uint8_t PACKET_TYPE_RESPONSE = 0x0A;
    const uint8_t PACKET_TYPE = PACKET_TYPE_REQUEST;

    // Payload ответа: ULTRASONIC_COUNT x uint16 (мм, little-endian), индекс = номер датчика (канал Cn)
    const uint16_t DISTANCE_NO_ECHO = 0x0000; // эхо не вернулось в пределах дальности
    const uint16_t DISTANCE_ERROR = 0xFFFF;   // датчик не ответил или ещё не измерялся

    void Init();
    void Reset();
    void Handler(const ComLinkRTProtocol::PacketHeader& header, const uint8_t* data);
    void Processor();
}
