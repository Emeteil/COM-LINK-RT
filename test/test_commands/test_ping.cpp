#include <gtest/gtest.h>
#include "commands_fixture.h"

using TestSupport::CommandsFixture;
using TestSupport::ExpectPacket;
using TestSupport::PacketBuilder;

namespace
{
    class PingTest : public CommandsFixture
    {
    };
}

TEST_F(PingTest, AnswersWithResponsePacketType)
{
    Exchange(PacketBuilder(CommandPing::PACKET_TYPE, 0x1234));

    ASSERT_EQ(SentCount(), 1u);
    ExpectPacket(LastSent(), CommandPing::PACKET_TYPE_RESPONSE, 0x1234);
}

TEST_F(PingTest, AnswerCarriesNoPayload)
{
    Exchange(PacketBuilder(CommandPing::PACKET_TYPE, 0x0001));

    ASSERT_EQ(SentCount(), 1u);
    EXPECT_EQ(LastSent().header.dataLength, 0u);
    EXPECT_TRUE(LastSent().payload.empty());
}

TEST_F(PingTest, KeepsRequestPacketId)
{
    Exchange(PacketBuilder(CommandPing::PACKET_TYPE, 0xBEEF));

    ASSERT_EQ(SentCount(), 1u);
    EXPECT_EQ(LastSent().header.packetId, 0xBEEF);
}

TEST_F(PingTest, StaysSilentForZeroPacketId)
{
    Exchange(PacketBuilder(CommandPing::PACKET_TYPE, ZERO_PACKET_ID));

    EXPECT_EQ(SentCount(), 0u);
}

TEST_F(PingTest, IgnoresPayloadInRequest)
{
    Exchange(PacketBuilder(CommandPing::PACKET_TYPE, 0x0001).Payload({0x01, 0x02, 0x03}));

    ASSERT_EQ(SentCount(), 1u);
    EXPECT_EQ(LastSent().header.dataLength, 0u);
}

TEST_F(PingTest, AnswersEveryRequest)
{
    Exchange(PacketBuilder(CommandPing::PACKET_TYPE, 0x0001));
    Exchange(PacketBuilder(CommandPing::PACKET_TYPE, 0x0002));

    ASSERT_EQ(SentCount(), 2u);
    EXPECT_EQ(channel.Sent(0).header.packetId, 0x0001);
    EXPECT_EQ(channel.Sent(1).header.packetId, 0x0002);
}

TEST_F(PingTest, DoesNotAnswerBrokenPacket)
{
    Exchange(PacketBuilder(CommandPing::PACKET_TYPE, 0x0001).BreakCrc());

    EXPECT_EQ(SentCount(), 0u);
}
