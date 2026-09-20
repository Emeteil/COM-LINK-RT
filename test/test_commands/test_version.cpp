#include <gtest/gtest.h>
#include <string.h>
#include "commands_fixture.h"

using TestSupport::CommandsFixture;
using TestSupport::ExpectPacket;
using TestSupport::PacketBuilder;

namespace
{
    class VersionTest : public CommandsFixture
    {
    };
}

TEST_F(VersionTest, AnswersWithResponsePacketType)
{
    Exchange(PacketBuilder(CommandVersion::PACKET_TYPE, 0x0042));

    ASSERT_EQ(SentCount(), 1u);
    ExpectPacket(LastSent(), CommandVersion::PACKET_TYPE_RESPONSE, 0x0042);
}

TEST_F(VersionTest, PayloadSizeMatchesVersionInfo)
{
    Exchange(PacketBuilder(CommandVersion::PACKET_TYPE, 0x0001));

    ASSERT_EQ(SentCount(), 1u);
    EXPECT_EQ(LastSent().header.dataLength, sizeof(CommandVersion::VersionInfo));
    EXPECT_EQ(LastSent().payload.size(), sizeof(CommandVersion::VersionInfo));
}

TEST_F(VersionTest, ReportsBuildDateInCompilerFormat)
{
    Exchange(PacketBuilder(CommandVersion::PACKET_TYPE, 0x0001));

    ASSERT_EQ(SentCount(), 1u);
    CommandVersion::VersionInfo info = LastSent().PayloadAs<CommandVersion::VersionInfo>();

    EXPECT_EQ(strlen(info.buildDate), strlen(__DATE__));
    EXPECT_EQ(info.buildDate[3], ' ');
    EXPECT_EQ(info.buildDate[6], ' ');
}

TEST_F(VersionTest, ReportsBuildTimeInCompilerFormat)
{
    Exchange(PacketBuilder(CommandVersion::PACKET_TYPE, 0x0001));

    ASSERT_EQ(SentCount(), 1u);
    CommandVersion::VersionInfo info = LastSent().PayloadAs<CommandVersion::VersionInfo>();

    EXPECT_EQ(strlen(info.buildTime), strlen(__TIME__));
    EXPECT_EQ(info.buildTime[2], ':');
    EXPECT_EQ(info.buildTime[5], ':');
}

TEST_F(VersionTest, StringsAreNullTerminated)
{
    Exchange(PacketBuilder(CommandVersion::PACKET_TYPE, 0x0001));

    ASSERT_EQ(SentCount(), 1u);
    CommandVersion::VersionInfo info = LastSent().PayloadAs<CommandVersion::VersionInfo>();

    EXPECT_EQ(info.buildDate[sizeof(info.buildDate) - 1], '\0');
    EXPECT_EQ(info.buildTime[sizeof(info.buildTime) - 1], '\0');
}

TEST_F(VersionTest, StaysSilentForZeroPacketId)
{
    Exchange(PacketBuilder(CommandVersion::PACKET_TYPE, ZERO_PACKET_ID));

    EXPECT_EQ(SentCount(), 0u);
}

TEST_F(VersionTest, ProtocolVersionInAnswerMatchesCurrentVersion)
{
    Exchange(PacketBuilder(CommandVersion::PACKET_TYPE, 0x0001));

    ASSERT_EQ(SentCount(), 1u);
    EXPECT_EQ(LastSent().header.version, PROTOCOL_VERSION);
}
