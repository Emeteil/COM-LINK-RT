#include <gtest/gtest.h>
#include "commands_fixture.h"

using TestSupport::CommandsFixture;
using TestSupport::ExpectPacket;
using TestSupport::PacketBuilder;

namespace
{
    const unsigned long SEND_INTERVAL_MS = 3000;
    const unsigned long KEEP_ALIVE_TIMEOUT_MS = 10000;

    class MillisTest : public CommandsFixture
    {
        protected:
        uint32_t LastMillisValue() const { return LastSent().PayloadAs<uint32_t>(); }

        void Subscribe(uint16_t packetId)
        {
            Exchange(PacketBuilder(CommandMillis::PACKET_TYPE, packetId).ServiceBits(SERVICE_BIT_SUBSCRIBE));
        }
    };
}

TEST_F(MillisTest, SingleRequestAnswersWithCurrentMillis)
{
    Advance(1234);

    Exchange(PacketBuilder(CommandMillis::PACKET_TYPE, 0x0007));

    ASSERT_EQ(SentCount(), 1u);
    ExpectPacket(LastSent(), CommandMillis::PACKET_TYPE_RESPONSE, 0x0007);
    EXPECT_EQ(LastSent().header.dataLength, sizeof(uint32_t));
    EXPECT_EQ(LastMillisValue(), 1234u);
}

TEST_F(MillisTest, SingleRequestDoesNotStartSubscription)
{
    Exchange(PacketBuilder(CommandMillis::PACKET_TYPE, 0x0007));
    size_t before = SentCount();

    Tick(SEND_INTERVAL_MS * 3);

    EXPECT_EQ(SentCount(), before);
}

TEST_F(MillisTest, StaysSilentForZeroPacketId)
{
    Exchange(PacketBuilder(CommandMillis::PACKET_TYPE, ZERO_PACKET_ID));

    EXPECT_EQ(SentCount(), 0u);
}

TEST_F(MillisTest, SubscribeIsAcknowledged)
{
    Subscribe(0x0011);

    ASSERT_EQ(SentCount(), 1u);
    ExpectPacket(LastSent(), CommandMillis::PACKET_TYPE_RESPONSE, 0x0011);
}

TEST_F(MillisTest, SubscriptionSendsAfterInterval)
{
    Subscribe(0x0011);

    Tick(SEND_INTERVAL_MS + 1);

    ASSERT_EQ(SentCount(), 2u);
    EXPECT_EQ(LastSent().header.packetId, 0x0011);
    EXPECT_EQ(LastMillisValue(), SEND_INTERVAL_MS + 1);
}

TEST_F(MillisTest, SubscriptionIsSilentBeforeInterval)
{
    Subscribe(0x0011);

    Tick(SEND_INTERVAL_MS - 1);

    EXPECT_EQ(SentCount(), 1u);
}

TEST_F(MillisTest, SubscriptionKeepsSendingPeriodically)
{
    Subscribe(0x0011);

    for (int i = 0; i < 3; i++)
        Tick(SEND_INTERVAL_MS + 1);

    EXPECT_EQ(SentCount(), 4u);
}

TEST_F(MillisTest, SubscriptionTimesOutWithoutKeepAlive)
{
    Subscribe(0x0011);

    Tick(KEEP_ALIVE_TIMEOUT_MS + 1);

    EXPECT_EQ(LastSent().header.serviceBits, SERVICE_BIT_UNSUBSCRIBED);

    size_t after = SentCount();
    Tick(SEND_INTERVAL_MS + 1);
    EXPECT_EQ(SentCount(), after);
}

TEST_F(MillisTest, KeepAliveExtendsSubscription)
{
    Subscribe(0x0011);

    Tick(KEEP_ALIVE_TIMEOUT_MS - 1000);
    Exchange(PacketBuilder(CommandMillis::PACKET_TYPE, 0x0011).ServiceBits(SERVICE_BIT_KEEP_ALIVE));
    Tick(KEEP_ALIVE_TIMEOUT_MS - 1000);

    EXPECT_NE(LastSent().header.serviceBits, SERVICE_BIT_UNSUBSCRIBED);
}

TEST_F(MillisTest, UnsubscribeIsAcknowledgedAndStopsStream)
{
    Subscribe(0x0011);

    Exchange(PacketBuilder(CommandMillis::PACKET_TYPE, 0x0011).ServiceBits(SERVICE_BIT_UNSUBSCRIBE));

    EXPECT_EQ(LastSent().header.serviceBits, SERVICE_BIT_UNSUBSCRIBED);

    size_t after = SentCount();
    Tick(SEND_INTERVAL_MS + 1);
    EXPECT_EQ(SentCount(), after);
}

TEST_F(MillisTest, ResetStopsSubscription)
{
    Subscribe(0x0011);
    Tick(SEND_INTERVAL_MS + 1);

    CommandMillis::Reset();

    size_t after = SentCount();
    Tick(SEND_INTERVAL_MS + 1);
    EXPECT_EQ(SentCount(), after);
}

TEST_F(MillisTest, ReportedMillisGrowsWithTime)
{
    Subscribe(0x0011);

    Tick(SEND_INTERVAL_MS + 1);
    uint32_t first = LastMillisValue();

    Tick(SEND_INTERVAL_MS + 1);
    uint32_t second = LastMillisValue();

    EXPECT_GT(second, first);
}
