#include <gtest/gtest.h>
#include <stdio.h>
#include <string>
#include "commands_fixture.h"

using CommandGps::GpsInfo;
using CommandGps::GpsSatellite;
using TestSupport::CommandsFixture;
using TestSupport::ExpectPacket;
using TestSupport::PacketBuilder;

namespace
{
    const char* RMC = "GPRMC,123519.00,A,5501.12345,N,08254.67890,E,1.500,84.40,181026,,,A";
    const char* GGA = "GPGGA,123519.00,5501.12345,N,08254.67890,E,1,08,0.92,152.3,M,-25.1,M,,";
    const char* GSA = "GPGSA,A,3,04,05,09,12,,,,,,,,,1.65,0.92,1.37";
    const char* GSV1 = "GPGSV,2,1,05,04,45,120,38,05,30,200,25,09,10,045,,12,60,300,41";
    const char* GSV2 = "GPGSV,2,2,05,17,05,010,";

    std::string Sentence(const std::string& body)
    {
        uint8_t checksum = 0;
        for (char c : body)
            checksum ^= static_cast<uint8_t>(c);
        char tail[8];
        snprintf(tail, sizeof(tail), "*%02X\r\n", checksum);
        return "$" + body + tail;
    }

    class GpsTest : public CommandsFixture
    {
        protected:
        void Send(const std::string& raw)
        {
            CommandGps::Port().Feed(reinterpret_cast<const uint8_t*>(raw.data()), raw.size());
            Tick();
        }

        void SendAll()
        {
            for (const char* body : {RMC, GGA, GSA, GSV1, GSV2})
                Send(Sentence(body));
        }

        GpsInfo Request()
        {
            Exchange(PacketBuilder(CommandGps::PACKET_TYPE, 0x0031));
            return LastSent().PayloadAs<GpsInfo>();
        }

        GpsSatellite Satellite(size_t index) const
        {
            GpsSatellite sat;
            memcpy(&sat, LastSent().payload.data() + sizeof(GpsInfo) + index * sizeof(GpsSatellite), sizeof(sat));
            return sat;
        }
    };
}

TEST_F(GpsTest, OpensSerialAtNmeaBaudRate)
{
    EXPECT_TRUE(CommandGps::Port().started);
    EXPECT_EQ(CommandGps::Port().baudRate, GPS_BAUD_RATE);
}

TEST_F(GpsTest, ReportsNothingBeforeModuleSpeaks)
{
    GpsInfo info = Request();

    ExpectPacket(LastSent(), CommandGps::PACKET_TYPE_RESPONSE, 0x0031);
    EXPECT_EQ(info.flags, 0);
    EXPECT_EQ(info.sentenceAgeMs, CommandGps::AGE_NEVER);
    EXPECT_EQ(info.fixAgeMs, CommandGps::AGE_NEVER);
    EXPECT_EQ(info.satelliteCount, 0);
    EXPECT_EQ(LastSent().payload.size(), sizeof(GpsInfo));
}

TEST_F(GpsTest, DecodesFullFix)
{
    SendAll();
    GpsInfo info = Request();

    EXPECT_EQ(info.flags, CommandGps::FLAG_ALIVE | CommandGps::FLAG_POSITION_VALID | CommandGps::FLAG_TIME_VALID | CommandGps::FLAG_DATE_VALID);
    EXPECT_EQ(info.latitudeE7, 550187242);
    EXPECT_EQ(info.longitudeE7, 829113150);
    EXPECT_EQ(info.altitudeCm, 15230);
    EXPECT_EQ(info.geoidSeparationCm, -2510);
    EXPECT_EQ(info.speedMmS, 772u);
    EXPECT_EQ(info.courseCdeg, 8440);
    EXPECT_EQ(info.fixQuality, 1);
    EXPECT_EQ(info.fixType, 3);
    EXPECT_EQ(info.satellitesUsed, 8);
    EXPECT_EQ(info.pdopCenti, 165);
    EXPECT_EQ(info.hdopCenti, 92);
    EXPECT_EQ(info.vdopCenti, 137);
    EXPECT_EQ(info.day, 18);
    EXPECT_EQ(info.month, 10);
    EXPECT_EQ(info.year, 26);
    EXPECT_EQ(info.hour, 12);
    EXPECT_EQ(info.minute, 35);
    EXPECT_EQ(info.second, 19);
    EXPECT_EQ(info.sentencesOk, 5);
    EXPECT_EQ(info.checksumErrors, 0);
}

TEST_F(GpsTest, ListsSatellitesInView)
{
    SendAll();
    GpsInfo info = Request();

    ASSERT_EQ(info.satelliteCount, 5);
    EXPECT_EQ(info.satellitesInView, 5);
    EXPECT_EQ(LastSent().payload.size(), sizeof(GpsInfo) + 5 * sizeof(GpsSatellite));

    GpsSatellite first = Satellite(0);
    EXPECT_EQ(first.prn, 4);
    EXPECT_EQ(first.elevation, 45);
    EXPECT_EQ(first.azimuth, 120);
    EXPECT_EQ(first.snr, 38);

    EXPECT_EQ(Satellite(2).snr, 0);
    EXPECT_EQ(Satellite(4).prn, 17);
    EXPECT_EQ(Satellite(4).azimuth, 10);
}

TEST_F(GpsTest, SouthAndWestAreNegative)
{
    Send(Sentence("GPRMC,000000.00,A,3352.12800,S,15112.55800,W,0.0,0.0,010126,,,A"));
    GpsInfo info = Request();

    EXPECT_EQ(info.latitudeE7, -338688000);
    EXPECT_EQ(info.longitudeE7, -1512093000);
}

TEST_F(GpsTest, NoFixKeepsModuleAliveButPositionInvalid)
{
    Send(Sentence("GPRMC,081836.00,V,,,,,,,181026,,,N"));
    Send(Sentence("GPGGA,081836.00,,,,,0,00,99.99,,,,,,"));
    GpsInfo info = Request();

    EXPECT_EQ(info.flags & CommandGps::FLAG_ALIVE, CommandGps::FLAG_ALIVE);
    EXPECT_EQ(info.flags & CommandGps::FLAG_POSITION_VALID, 0);
    EXPECT_EQ(info.fixQuality, 0);
    EXPECT_EQ(info.hdopCenti, 9999);
    EXPECT_EQ(info.fixAgeMs, CommandGps::AGE_NEVER);
}

TEST_F(GpsTest, BadChecksumIsCountedAndIgnored)
{
    std::string broken = Sentence(RMC);
    broken[10] = '9';
    Send(broken);
    GpsInfo info = Request();

    EXPECT_EQ(info.checksumErrors, 1);
    EXPECT_EQ(info.sentencesOk, 0);
    EXPECT_EQ(info.flags, 0);
}

TEST_F(GpsTest, AliveFlagDropsWhenModuleGoesSilent)
{
    SendAll();
    Tick(CommandGps::ALIVE_TIMEOUT_MS + 10);
    GpsInfo info = Request();

    EXPECT_EQ(info.flags & CommandGps::FLAG_ALIVE, 0);
    EXPECT_GE(info.sentenceAgeMs, CommandGps::ALIVE_TIMEOUT_MS);
    EXPECT_EQ(info.flags & CommandGps::FLAG_POSITION_VALID, CommandGps::FLAG_POSITION_VALID);
}

TEST_F(GpsTest, SentenceSplitAcrossReadsIsAssembled)
{
    std::string raw = Sentence(GGA);
    Send(raw.substr(0, 20));
    Send(raw.substr(20));

    EXPECT_EQ(Request().satellitesUsed, 8);
}

TEST_F(GpsTest, StaysSilentForZeroPacketId)
{
    Exchange(PacketBuilder(CommandGps::PACKET_TYPE, ZERO_PACKET_ID));

    EXPECT_EQ(SentCount(), 0u);
}

TEST(NmeaParse, FixedPointHandlesPaddingAndTruncation)
{
    int64_t value = 0;
    ASSERT_TRUE(Nmea::ParseFixed("1.5", 3, value));
    EXPECT_EQ(value, 1500);
    ASSERT_TRUE(Nmea::ParseFixed("-25.123", 1, value));
    EXPECT_EQ(value, -251);
    EXPECT_FALSE(Nmea::ParseFixed("", 2, value));
    EXPECT_FALSE(Nmea::ParseFixed("1a", 2, value));
}
