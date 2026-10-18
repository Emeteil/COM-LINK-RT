#include "gps.h"
#include "../core/globals.h"
#include <string.h>

namespace CommandGps
{
    static constexpr uint16_t MAX_READ_PER_TICK = 128;

    static Uart gpsSerial(GPS_RX_PIN, GPS_TX_PIN);
    static Nmea::Parser parser;
    static bool everAlive = false;
    static bool everFixed = false;
    static unsigned long lastSentenceMs = 0;
    static unsigned long lastFixMs = 0;

    Uart& Port()
    {
        return gpsSerial;
    }

    static uint32_t AgeSince(bool ever, unsigned long since, unsigned long now)
    {
        return ever ? static_cast<uint32_t>(now - since) : AGE_NEVER;
    }

    static uint16_t BuildPayload(uint8_t* out)
    {
        const Nmea::GpsState& s = parser.State();
        const unsigned long now = millis();

        GpsInfo info;
        memset(&info, 0, sizeof(info));
        info.sentenceAgeMs = AgeSince(everAlive, lastSentenceMs, now);
        info.fixAgeMs = AgeSince(everFixed, lastFixMs, now);
        info.flags = (everAlive && info.sentenceAgeMs < ALIVE_TIMEOUT_MS) ? FLAG_ALIVE : 0;
        if (s.positionValid)
            info.flags |= FLAG_POSITION_VALID;
        if (s.timeValid)
            info.flags |= FLAG_TIME_VALID;
        if (s.dateValid)
            info.flags |= FLAG_DATE_VALID;

        info.fixQuality = s.fixQuality;
        info.fixType = s.fixType;
        info.satellitesUsed = s.satellitesUsed;
        info.latitudeE7 = s.latitudeE7;
        info.longitudeE7 = s.longitudeE7;
        info.altitudeCm = s.altitudeCm;
        info.geoidSeparationCm = s.geoidSeparationCm;
        info.speedMmS = s.speedMmS;
        info.courseCdeg = s.courseCdeg;
        info.hdopCenti = s.hdopCenti;
        info.pdopCenti = s.pdopCenti;
        info.vdopCenti = s.vdopCenti;
        info.year = s.year;
        info.month = s.month;
        info.day = s.day;
        info.hour = s.hour;
        info.minute = s.minute;
        info.second = s.second;
        info.centisecond = s.centisecond;
        info.sentencesOk = s.sentencesOk;
        info.checksumErrors = s.checksumErrors;
        info.satellitesInView = s.satellitesInView;
        info.satelliteCount = s.satelliteCount;

        memcpy(out, &info, sizeof(info));
        uint16_t length = sizeof(info);
        for (uint8_t i = 0; i < s.satelliteCount; i++)
        {
            GpsSatellite sat;
            sat.prn = s.satellites[i].prn;
            sat.elevation = s.satellites[i].elevation;
            sat.azimuth = s.satellites[i].azimuth;
            sat.snr = s.satellites[i].snr;
            memcpy(out + length, &sat, sizeof(sat));
            length += sizeof(sat);
        }
        return length;
    }

    void Reset()
    {
        parser.Reset();
        everAlive = false;
        everFixed = false;
        lastSentenceMs = 0;
        lastFixMs = 0;
    }

    void Init()
    {
        Reset();
        gpsSerial.begin(GPS_BAUD_RATE);
    }

    void Handler(const ComLinkRTProtocol::PacketHeader& header, const uint8_t* data)
    {
        if (header.packetId == ZERO_PACKET_ID)
            return;

        uint8_t payload[sizeof(GpsInfo) + sizeof(GpsSatellite) * Nmea::MAX_SATELLITES];
        uint8_t txBuffer[sizeof(payload) + sizeof(ComLinkRTProtocol::PacketHeader)];
        uint16_t length;

        protocol.parser.CreatePacket(
            PROTOCOL_VERSION,
            PACKET_TYPE_RESPONSE,
            EMPTY_SERVICE_BITS,
            header.packetId,
            payload,
            BuildPayload(payload),
            txBuffer,
            length);

        protocol.SendPacket(txBuffer, length, header.packetId);
    }

    void Processor()
    {
        for (uint16_t i = 0; i < MAX_READ_PER_TICK && gpsSerial.available() > 0; i++)
        {
            if (!parser.Feed(static_cast<char>(gpsSerial.read())))
                continue;

            lastSentenceMs = millis();
            everAlive = true;
            if (parser.State().positionValid)
            {
                lastFixMs = lastSentenceMs;
                everFixed = true;
            }
        }
    }
}
