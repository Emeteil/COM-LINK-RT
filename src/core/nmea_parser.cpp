#include "nmea_parser.h"
#include <string.h>

namespace Nmea
{
    static int HexValue(char c)
    {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        return -1;
    }

    bool ParseFixed(const char* text, uint8_t decimals, int64_t& out)
    {
        if (text == nullptr || *text == '\0')
            return false;

        bool negative = false;
        if (*text == '-' || *text == '+')
            negative = *text++ == '-';

        int64_t value = 0;
        bool digits = false;
        while (*text >= '0' && *text <= '9')
        {
            value = value * 10 + (*text++ - '0');
            digits = true;
        }

        uint8_t used = 0;
        if (*text == '.')
        {
            text++;
            while (*text >= '0' && *text <= '9')
            {
                if (used < decimals)
                {
                    value = value * 10 + (*text - '0');
                    used++;
                }
                digits = true;
                text++;
            }
        }
        if (!digits || *text != '\0')
            return false;

        for (; used < decimals; used++)
            value *= 10;

        out = negative ? -value : value;
        return true;
    }

    bool ParseCoordinate(const char* value, const char* hemisphere, int32_t& outE7)
    {
        int64_t raw = 0;
        if (!ParseFixed(value, 5, raw) || raw < 0 || hemisphere == nullptr)
            return false;

        const int64_t degrees = raw / 10000000LL;
        const int64_t minutesE5 = raw % 10000000LL;
        int64_t e7 = degrees * 10000000LL + (minutesE5 * 100 + 30) / 60;

        if (*hemisphere == 'S' || *hemisphere == 'W')
            e7 = -e7;
        else if (*hemisphere != 'N' && *hemisphere != 'E')
            return false;

        outE7 = static_cast<int32_t>(e7);
        return true;
    }

    static bool ParseUnsigned(const char* text, uint32_t& out)
    {
        int64_t value = 0;
        if (!ParseFixed(text, 0, value) || value < 0)
            return false;
        out = static_cast<uint32_t>(value);
        return true;
    }

    static bool ParseTime(const char* text, uint8_t& hour, uint8_t& minute, uint8_t& second, uint8_t& centi)
    {
        int64_t value = 0;
        if (strlen(text) < 6 || !ParseFixed(text, 2, value) || value < 0)
            return false;
        centi = static_cast<uint8_t>(value % 100);
        value /= 100;
        second = static_cast<uint8_t>(value % 100);
        minute = static_cast<uint8_t>((value / 100) % 100);
        hour = static_cast<uint8_t>(value / 10000);
        return hour < 24 && minute < 60 && second < 61;
    }

    static uint16_t ToCenti(const char* text)
    {
        int64_t value = 0;
        if (!ParseFixed(text, 2, value) || value < 0)
            return 0;
        return value > 0xFFFF ? 0xFFFF : static_cast<uint16_t>(value);
    }

    Parser::Parser()
    {
        Reset();
    }

    void Parser::Reset()
    {
        memset(&state, 0, sizeof(state));
        length = 0;
        collecting = false;
        pendingCount = 0;
    }

    bool Parser::Feed(char c)
    {
        if (c == '$')
        {
            collecting = true;
            length = 0;
            buffer[length++] = c;
            return false;
        }
        if (!collecting)
            return false;

        if (c == '\r' || c == '\n')
        {
            collecting = false;
            buffer[length] = '\0';
            return Process();
        }

        if (length >= MAX_SENTENCE)
        {
            collecting = false;
            return false;
        }
        buffer[length++] = c;
        return false;
    }

    bool Parser::Process()
    {
        char* star = strchr(buffer, '*');
        if (star == nullptr || star[1] == '\0' || star[2] == '\0')
            return false;

        const int high = HexValue(star[1]);
        const int low = HexValue(star[2]);
        uint8_t checksum = 0;
        for (const char* p = buffer + 1; p < star; p++)
            checksum ^= static_cast<uint8_t>(*p);

        if (high < 0 || low < 0 || checksum != ((high << 4) | low))
        {
            state.checksumErrors++;
            return false;
        }
        *star = '\0';

        char* fields[MAX_FIELDS];
        uint8_t count = 0;
        char* cursor = buffer + 1;
        fields[count++] = cursor;
        while ((cursor = strchr(cursor, ',')) != nullptr && count < MAX_FIELDS)
        {
            *cursor++ = '\0';
            fields[count++] = cursor;
        }

        if (strlen(fields[0]) != 5)
            return false;

        const char* type = fields[0] + 2;
        state.sentencesOk++;
        if (strcmp(type, "GGA") == 0)
            HandleGga(fields, count);
        else if (strcmp(type, "RMC") == 0)
            HandleRmc(fields, count);
        else if (strcmp(type, "GSA") == 0)
            HandleGsa(fields, count);
        else if (strcmp(type, "GSV") == 0)
            HandleGsv(fields, count);
        return true;
    }

    void Parser::HandleGga(char** fields, uint8_t count)
    {
        if (count < 12)
            return;

        uint32_t quality = 0;
        if (ParseUnsigned(fields[6], quality))
            state.fixQuality = static_cast<uint8_t>(quality);

        uint32_t used = 0;
        state.satellitesUsed = ParseUnsigned(fields[7], used) ? static_cast<uint8_t>(used) : 0;
        state.hdopCenti = ToCenti(fields[8]);

        int64_t value = 0;
        if (ParseFixed(fields[9], 2, value))
            state.altitudeCm = static_cast<int32_t>(value);
        if (ParseFixed(fields[11], 2, value))
            state.geoidSeparationCm = static_cast<int32_t>(value);

        if (state.fixQuality > 0)
        {
            int32_t lat = 0;
            int32_t lon = 0;
            if (ParseCoordinate(fields[2], fields[3], lat) && ParseCoordinate(fields[4], fields[5], lon))
            {
                state.latitudeE7 = lat;
                state.longitudeE7 = lon;
                state.positionValid = true;
            }
        }
    }

    void Parser::HandleRmc(char** fields, uint8_t count)
    {
        if (count < 10)
            return;

        state.timeValid = ParseTime(fields[1], state.hour, state.minute, state.second, state.centisecond);

        uint32_t date = 0;
        state.dateValid = strlen(fields[9]) == 6 && ParseUnsigned(fields[9], date);
        if (state.dateValid && date == RECEIVER_DEFAULT_DATE)
            state.dateValid = state.timeValid = false;
        if (state.dateValid)
        {
            state.day = static_cast<uint8_t>(date / 10000);
            state.month = static_cast<uint8_t>((date / 100) % 100);
            state.year = static_cast<uint8_t>(date % 100);
        }

        if (fields[2][0] != 'A')
        {
            state.positionValid = false;
            state.speedMmS = 0;
            return;
        }

        int32_t lat = 0;
        int32_t lon = 0;
        if (ParseCoordinate(fields[3], fields[4], lat) && ParseCoordinate(fields[5], fields[6], lon))
        {
            state.latitudeE7 = lat;
            state.longitudeE7 = lon;
            state.positionValid = true;
        }

        int64_t knotsMilli = 0;
        if (ParseFixed(fields[7], 3, knotsMilli) && knotsMilli >= 0)
            state.speedMmS = static_cast<uint32_t>((knotsMilli * 514444LL + 500000LL) / 1000000LL);

        int64_t course = 0;
        if (ParseFixed(fields[8], 2, course) && course >= 0)
            state.courseCdeg = static_cast<uint16_t>(course % 36000);
    }

    void Parser::HandleGsa(char** fields, uint8_t count)
    {
        if (count < 18)
            return;

        uint32_t fixType = 0;
        if (ParseUnsigned(fields[2], fixType))
            state.fixType = static_cast<uint8_t>(fixType);
        state.pdopCenti = ToCenti(fields[15]);
        state.hdopCenti = ToCenti(fields[16]);
        state.vdopCenti = ToCenti(fields[17]);
    }

    void Parser::HandleGsv(char** fields, uint8_t count)
    {
        uint32_t total = 0;
        uint32_t index = 0;
        uint32_t inView = 0;
        if (count < 4 || !ParseUnsigned(fields[1], total) || !ParseUnsigned(fields[2], index) || !ParseUnsigned(fields[3], inView))
            return;

        if (index == 1)
            pendingCount = 0;

        for (uint8_t f = 4; f < count; f += 4)
        {
            uint32_t prn = 0;
            if (!ParseUnsigned(fields[f], prn) || pendingCount >= MAX_SATELLITES)
                continue;

            Satellite& sat = pending[pendingCount++];
            uint32_t value = 0;
            sat.prn = static_cast<uint8_t>(prn);
            sat.elevation = (f + 1 < count && ParseUnsigned(fields[f + 1], value)) ? static_cast<int8_t>(value) : -1;
            sat.azimuth = (f + 2 < count && ParseUnsigned(fields[f + 2], value)) ? static_cast<uint16_t>(value) : 0xFFFF;
            sat.snr = (f + 3 < count && ParseUnsigned(fields[f + 3], value)) ? static_cast<uint8_t>(value) : 0;
        }

        if (index == total)
        {
            state.satellitesInView = static_cast<uint8_t>(inView);
            state.satelliteCount = pendingCount;
            memcpy(state.satellites, pending, sizeof(Satellite) * pendingCount);
        }
    }
}
