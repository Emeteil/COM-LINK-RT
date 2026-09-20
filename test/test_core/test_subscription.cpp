#include <gtest/gtest.h>
#include <vector>
#include "../../src/core/subscription_base.h"
#include "arduino/mock_state.h"

namespace
{
    const unsigned long SEND_INTERVAL_MS = 3000;
    const unsigned long KEEP_ALIVE_TIMEOUT_MS = 10000;
    const uint8_t RESPONSE_TYPE = 0x04;

    struct Response
    {
        uint8_t packetType;
        uint16_t packetId;
        uint8_t serviceBits;
        unsigned long timeMs;
    };

    class RecordingSubscription : public CommandSubscription::SubscriptionHandler
    {
        public:
        std::vector<Response> responses;

        RecordingSubscription() : SubscriptionHandler(SEND_INTERVAL_MS, KEEP_ALIVE_TIMEOUT_MS, RESPONSE_TYPE) {}

        protected:
        void SendResponse(uint8_t packetType, uint16_t packetId, uint8_t serviceBits) override
        {
            responses.push_back({packetType, packetId, serviceBits, millis()});
        }
    };

    ComLinkRTProtocol::PacketHeader MakeHeader(uint16_t packetId, uint8_t serviceBits)
    {
        ComLinkRTProtocol::PacketHeader header = {};
        header.syncByte1 = SYNC_BYTE1;
        header.syncByte2 = SYNC_BYTE2;
        header.version = PROTOCOL_VERSION;
        header.packetType = 0x03;
        header.serviceBits = serviceBits;
        header.packetId = packetId;
        header.dataLength = 0;
        return header;
    }

    class SubscriptionTest : public ::testing::Test
    {
        protected:
        RecordingSubscription subscription;

        void SetUp() override { MockArduino::ResetAll(); }

        void Send(uint16_t packetId, uint8_t serviceBits)
        {
            ComLinkRTProtocol::PacketHeader header = MakeHeader(packetId, serviceBits);
            subscription.HandleSubscription(header, nullptr);
        }

        void Advance(unsigned long ms) { MockArduino::Advance(ms); }

        void Process() { subscription.ProcessSubscription(); }

        void AdvanceAndProcess(unsigned long ms)
        {
            Advance(ms);
            Process();
        }

        size_t ResponseCount() const { return subscription.responses.size(); }

        const Response& LastResponse() const { return subscription.responses.back(); }
    };
}

TEST_F(SubscriptionTest, StartsInactive)
{
    EXPECT_FALSE(subscription.IsActive());
    EXPECT_EQ(ResponseCount(), 0u);
}

TEST_F(SubscriptionTest, PlainRequestAnswersOnceWithoutActivating)
{
    Send(0x0011, EMPTY_SERVICE_BITS);

    ASSERT_EQ(ResponseCount(), 1u);
    EXPECT_EQ(LastResponse().packetType, RESPONSE_TYPE);
    EXPECT_EQ(LastResponse().packetId, 0x0011);
    EXPECT_EQ(LastResponse().serviceBits, EMPTY_SERVICE_BITS);
    EXPECT_FALSE(subscription.IsActive());
}

TEST_F(SubscriptionTest, SubscribeActivatesAndAnswers)
{
    Send(0x0011, SERVICE_BIT_SUBSCRIBE);

    EXPECT_TRUE(subscription.IsActive());
    ASSERT_EQ(ResponseCount(), 1u);
    EXPECT_EQ(LastResponse().packetId, 0x0011);
}

TEST_F(SubscriptionTest, SubscribeWithZeroPacketIdIsIgnored)
{
    Send(ZERO_PACKET_ID, SERVICE_BIT_SUBSCRIBE);

    EXPECT_FALSE(subscription.IsActive());
    EXPECT_EQ(ResponseCount(), 0u);
}

TEST_F(SubscriptionTest, RepeatedSubscribeIsIgnored)
{
    Send(0x0011, SERVICE_BIT_SUBSCRIBE);
    Send(0x0022, SERVICE_BIT_SUBSCRIBE);

    EXPECT_EQ(ResponseCount(), 1u);
    EXPECT_EQ(LastResponse().packetId, 0x0011);
}

TEST_F(SubscriptionTest, ProcessDoesNothingWhileInactive)
{
    AdvanceAndProcess(60000);

    EXPECT_EQ(ResponseCount(), 0u);
}

TEST_F(SubscriptionTest, DoesNotSendBeforeInterval)
{
    Send(0x0011, SERVICE_BIT_SUBSCRIBE);

    AdvanceAndProcess(SEND_INTERVAL_MS);

    EXPECT_EQ(ResponseCount(), 1u);
}

TEST_F(SubscriptionTest, SendsAfterInterval)
{
    Send(0x0011, SERVICE_BIT_SUBSCRIBE);

    AdvanceAndProcess(SEND_INTERVAL_MS + 1);

    ASSERT_EQ(ResponseCount(), 2u);
    EXPECT_EQ(LastResponse().packetId, 0x0011);
    EXPECT_EQ(LastResponse().serviceBits, EMPTY_SERVICE_BITS);
}

TEST_F(SubscriptionTest, SendsPeriodically)
{
    Send(0x0011, SERVICE_BIT_SUBSCRIBE);

    for (int i = 0; i < 3; i++)
        AdvanceAndProcess(SEND_INTERVAL_MS + 1);

    EXPECT_EQ(ResponseCount(), 4u);
    EXPECT_TRUE(subscription.IsActive());
}

TEST_F(SubscriptionTest, TimesOutWithoutKeepAlive)
{
    Send(0x0011, SERVICE_BIT_SUBSCRIBE);

    AdvanceAndProcess(KEEP_ALIVE_TIMEOUT_MS + 1);

    EXPECT_FALSE(subscription.IsActive());
    EXPECT_EQ(LastResponse().serviceBits, SERVICE_BIT_UNSUBSCRIBED);
    EXPECT_EQ(LastResponse().packetId, 0x0011);
}

TEST_F(SubscriptionTest, KeepAliveExtendsSubscription)
{
    Send(0x0011, SERVICE_BIT_SUBSCRIBE);

    AdvanceAndProcess(KEEP_ALIVE_TIMEOUT_MS - 1000);
    Send(0x0011, SERVICE_BIT_KEEP_ALIVE);
    AdvanceAndProcess(KEEP_ALIVE_TIMEOUT_MS - 1000);

    EXPECT_TRUE(subscription.IsActive());
}

TEST_F(SubscriptionTest, KeepAliveDoesNotProduceResponse)
{
    Send(0x0011, SERVICE_BIT_SUBSCRIBE);
    size_t before = ResponseCount();

    Send(0x0011, SERVICE_BIT_KEEP_ALIVE);

    EXPECT_EQ(ResponseCount(), before);
}

TEST_F(SubscriptionTest, KeepAliveWithForeignPacketIdDoesNotExtend)
{
    Send(0x0011, SERVICE_BIT_SUBSCRIBE);

    AdvanceAndProcess(KEEP_ALIVE_TIMEOUT_MS - 1000);
    Send(0x0099, SERVICE_BIT_KEEP_ALIVE);
    AdvanceAndProcess(2000);

    EXPECT_FALSE(subscription.IsActive());
    EXPECT_EQ(LastResponse().serviceBits, SERVICE_BIT_UNSUBSCRIBED);
}

TEST_F(SubscriptionTest, UnsubscribeIsAcknowledgedThenSubscriptionStops)
{
    Send(0x0011, SERVICE_BIT_SUBSCRIBE);

    Send(0x0011, SERVICE_BIT_UNSUBSCRIBE);

    EXPECT_EQ(LastResponse().serviceBits, SERVICE_BIT_UNSUBSCRIBED);
    EXPECT_TRUE(subscription.IsActive());

    Process();

    EXPECT_FALSE(subscription.IsActive());
}

TEST_F(SubscriptionTest, UnsubscribeWithForeignPacketIdIsIgnored)
{
    Send(0x0011, SERVICE_BIT_SUBSCRIBE);
    size_t before = ResponseCount();

    Send(0x0099, SERVICE_BIT_UNSUBSCRIBE);
    Process();

    EXPECT_EQ(ResponseCount(), before);
    EXPECT_TRUE(subscription.IsActive());
}

TEST_F(SubscriptionTest, StopsSendingAfterUnsubscribe)
{
    Send(0x0011, SERVICE_BIT_SUBSCRIBE);
    Send(0x0011, SERVICE_BIT_UNSUBSCRIBE);
    Process();

    size_t before = ResponseCount();
    AdvanceAndProcess(SEND_INTERVAL_MS + 1);

    EXPECT_EQ(ResponseCount(), before);
}

TEST_F(SubscriptionTest, CanSubscribeAgainAfterUnsubscribe)
{
    Send(0x0011, SERVICE_BIT_SUBSCRIBE);
    Send(0x0011, SERVICE_BIT_UNSUBSCRIBE);
    Process();

    Send(0x0022, SERVICE_BIT_SUBSCRIBE);

    EXPECT_TRUE(subscription.IsActive());
    EXPECT_EQ(LastResponse().packetId, 0x0022);
}

TEST_F(SubscriptionTest, ResetSubscriptionStopsEverything)
{
    Send(0x0011, SERVICE_BIT_SUBSCRIBE);

    subscription.ResetSubscription();

    EXPECT_FALSE(subscription.IsActive());

    size_t before = ResponseCount();
    AdvanceAndProcess(SEND_INTERVAL_MS + 1);
    EXPECT_EQ(ResponseCount(), before);
}

TEST_F(SubscriptionTest, PlainRequestWhileActiveDoesNotAnswer)
{
    Send(0x0011, SERVICE_BIT_SUBSCRIBE);
    size_t before = ResponseCount();

    Send(0x0011, EMPTY_SERVICE_BITS);

    EXPECT_EQ(ResponseCount(), before);
}
