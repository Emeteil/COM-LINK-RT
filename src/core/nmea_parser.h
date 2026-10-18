#pragma once
#include <stdint.h>

namespace Nmea
{
    static constexpr uint8_t MAX_SATELLITES = 16;
    static constexpr uint8_t MAX_SENTENCE = 96;
    static constexpr uint8_t MAX_FIELDS = 24;

    struct Satellite
    {
        uint8_t prn;
        int8_t elevation;
        uint16_t azimuth;
        uint8_t snr;
    };

    struct GpsState
    {
        bool positionValid;
        bool timeValid;
        bool dateValid;
        uint8_t fixQuality;
        uint8_t fixType;
        uint8_t satellitesUsed;
        uint8_t satellitesInView;
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
        uint16_t sentencesOk;
        uint16_t checksumErrors;
        uint8_t satelliteCount;
        Satellite satellites[MAX_SATELLITES];
    };

    class Parser
    {
        public:
        Parser();

        void Reset();
        bool Feed(char c);
        const GpsState& State() const { return state; }

        private:
        GpsState state;
        char buffer[MAX_SENTENCE + 1];
        uint8_t length;
        bool collecting;
        Satellite pending[MAX_SATELLITES];
        uint8_t pendingCount;

        bool Process();
        void HandleGga(char** fields, uint8_t count);
        void HandleRmc(char** fields, uint8_t count);
        void HandleGsa(char** fields, uint8_t count);
        void HandleGsv(char** fields, uint8_t count);
    };

    bool ParseFixed(const char* text, uint8_t decimals, int64_t& out);
    bool ParseCoordinate(const char* value, const char* hemisphere, int32_t& outE7);
}
