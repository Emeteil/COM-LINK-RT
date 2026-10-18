#pragma once
#include <Arduino.h>
#include "../protocol/com_protocol.h"
#include "../core/board_config.h"
#include "../core/nmea_parser.h"

namespace CommandGps
{
    const uint8_t PACKET_TYPE_REQUEST = 0x0D;
    const uint8_t PACKET_TYPE_RESPONSE = 0x0E;
    const uint8_t PACKET_TYPE = PACKET_TYPE_REQUEST;

    const uint32_t AGE_NEVER = 0xFFFFFFFFUL;
    const uint32_t ALIVE_TIMEOUT_MS = 2000UL;

    enum Flags : uint8_t
    {
        FLAG_ALIVE = 0x01,
        FLAG_POSITION_VALID = 0x02,
        FLAG_TIME_VALID = 0x04,
        FLAG_DATE_VALID = 0x08
    };

#pragma pack(push, 1)
    struct GpsSatellite
    {
        uint8_t prn;
        int8_t elevation;
        uint16_t azimuth;
        uint8_t snr;
    };

    struct GpsInfo
    {
        uint8_t flags;
        uint8_t fixQuality;
        uint8_t fixType;
        uint8_t satellitesUsed;
        int32_t latitudeE7;
        int32_t longitudeE7;
        int32_t altitudeCm;
        int32_t geoidSeparationCm;
        uint32_t speedMmS;
        uint16_t courseCdeg;
        uint16_t hdopCenti;
        uint16_t pdopCenti;
        uint16_t vdopCenti;
        uint8_t year;
        uint8_t month;
        uint8_t day;
        uint8_t hour;
        uint8_t minute;
        uint8_t second;
        uint8_t centisecond;
        uint32_t fixAgeMs;
        uint32_t sentenceAgeMs;
        uint16_t sentencesOk;
        uint16_t checksumErrors;
        uint8_t satellitesInView;
        uint8_t satelliteCount;
    };
#pragma pack(pop)

    Uart& Port();
    void Init();
    void Reset();
    void Handler(const ComLinkRTProtocol::PacketHeader& header, const uint8_t* data);
    void Processor();
}
