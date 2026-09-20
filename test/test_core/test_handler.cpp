#include <gtest/gtest.h>
#include <string.h>
#include <vector>
#include "../../src/protocol/com_protocol_handler.h"
#include "fake_channel.h"
#include "packet_builder.h"

using ComLinkRTProtocol::PacketHeader;
using ComLinkRTProtocol::ProtocolHandler;
using TestSupport::FakeChannel;
using TestSupport::PacketBuilder;

namespace
{
    struct Spy
    {
        unsigned int handlerCalls;
        unsigned int processorCalls;
        unsigned int initCalls;
        unsigned int otherHandlerCalls;
        PacketHeader lastHeader;
        std::vector<uint8_t> lastPayload;
        bool lastPayloadWasNull;
    };

    Spy spy;

    void ResetSpy()
    {
        spy.handlerCalls = 0;
        spy.processorCalls = 0;
        spy.initCalls = 0;
        spy.otherHandlerCalls = 0;
        spy.lastPayloadWasNull = false;
        spy.lastPayload.clear();
        memset(&spy.lastHeader, 0, sizeof(spy.lastHeader));
    }

    void RecordingHandler(const PacketHeader& header, const uint8_t* data)
    {
        spy.handlerCalls++;
        spy.lastHeader = header;
        spy.lastPayloadWasNull = (data == nullptr);

        if (data == nullptr)
            spy.lastPayload.clear();
        else
            spy.lastPayload.assign(data, data + header.dataLength);
    }

    void OtherHandler(const PacketHeader&, const uint8_t*)
    {
        spy.otherHandlerCalls++;
    }

    void RecordingProcessor()
    {
        spy.processorCalls++;
    }

    void RecordingInit()
    {
        spy.initCalls++;
    }

    class HandlerTest : public ::testing::Test
    {
        protected:
        FakeChannel channel;
        ProtocolHandler handler{channel};

        void SetUp() override { ResetSpy(); }

        void Deliver(const PacketBuilder& packet)
        {
            channel.Feed(packet.Build());
            handler.Update();
        }
    };
}

TEST_F(HandlerTest, BeginOpensChannel)
{
    handler.Begin();

    EXPECT_EQ(channel.BeginCount(), 1u);
    EXPECT_EQ(handler.parser.GetState(), ComLinkRTProtocol::ProtocolParser::State::SYNC1);
}

TEST_F(HandlerTest, AddHandlerCallsInitFunction)
{
    handler.AddHandler(0x01, RecordingHandler, nullptr, RecordingInit);

    EXPECT_EQ(spy.initCalls, 1u);
}

TEST_F(HandlerTest, AddHandlerToleratesNullInitFunction)
{
    handler.AddHandler(0x01, RecordingHandler, nullptr, nullptr);

    Deliver(PacketBuilder(0x01, 0x0001));

    EXPECT_EQ(spy.handlerCalls, 1u);
}

TEST_F(HandlerTest, RoutesPacketToMatchingHandler)
{
    handler.AddHandler(0x01, RecordingHandler, nullptr, nullptr);
    handler.AddHandler(0x02, OtherHandler, nullptr, nullptr);

    Deliver(PacketBuilder(0x01, 0x0005));

    EXPECT_EQ(spy.handlerCalls, 1u);
    EXPECT_EQ(spy.otherHandlerCalls, 0u);
    EXPECT_EQ(spy.lastHeader.packetId, 0x0005);
}

TEST_F(HandlerTest, PassesPayloadToHandler)
{
    handler.AddHandler(0x01, RecordingHandler, nullptr, nullptr);
    std::vector<uint8_t> payload = {0x01, 0x02, 0x03};

    Deliver(PacketBuilder(0x01, 0x0001).Payload(payload));

    EXPECT_FALSE(spy.lastPayloadWasNull);
    EXPECT_EQ(spy.lastPayload, payload);
}

TEST_F(HandlerTest, PassesNullPayloadWhenPacketHasNoData)
{
    handler.AddHandler(0x01, RecordingHandler, nullptr, nullptr);

    Deliver(PacketBuilder(0x01, 0x0001));

    EXPECT_TRUE(spy.lastPayloadWasNull);
    EXPECT_EQ(spy.lastHeader.dataLength, 0u);
}

TEST_F(HandlerTest, IgnoresUnregisteredPacketType)
{
    handler.AddHandler(0x01, RecordingHandler, nullptr, nullptr);

    Deliver(PacketBuilder(0x42, 0x0001));

    EXPECT_EQ(spy.handlerCalls, 0u);
    EXPECT_EQ(handler.parser.GetState(), ComLinkRTProtocol::ProtocolParser::State::SYNC1);
}

TEST_F(HandlerTest, IgnoresForeignProtocolVersion)
{
    handler.AddHandler(0x01, RecordingHandler, nullptr, nullptr);

    Deliver(PacketBuilder(0x01, 0x0001).Version(PROTOCOL_VERSION - 1));

    EXPECT_EQ(spy.handlerCalls, 0u);
    EXPECT_EQ(handler.parser.GetState(), ComLinkRTProtocol::ProtocolParser::State::SYNC1);
}

TEST_F(HandlerTest, ResetsParserAfterHandlingSoNextPacketIsParsed)
{
    handler.AddHandler(0x01, RecordingHandler, nullptr, nullptr);

    Deliver(PacketBuilder(0x01, 0x0001));
    Deliver(PacketBuilder(0x01, 0x0002));

    EXPECT_EQ(spy.handlerCalls, 2u);
    EXPECT_EQ(spy.lastHeader.packetId, 0x0002);
}

TEST_F(HandlerTest, HandlesTwoPacketsArrivingInOneUpdate)
{
    handler.AddHandler(0x01, RecordingHandler, nullptr, nullptr);

    channel.Feed(PacketBuilder(0x01, 0x0001).Build());
    channel.Feed(PacketBuilder(0x01, 0x0002).Build());
    handler.Update();

    EXPECT_EQ(spy.handlerCalls, 2u);
}

TEST_F(HandlerTest, IgnoresBrokenPacketButAcceptsNextOne)
{
    handler.AddHandler(0x01, RecordingHandler, nullptr, nullptr);

    channel.Feed(PacketBuilder(0x01, 0x0001).BreakCrc().Build());
    channel.Feed(PacketBuilder(0x01, 0x0002).Build());
    handler.Update();

    EXPECT_EQ(spy.handlerCalls, 1u);
    EXPECT_EQ(spy.lastHeader.packetId, 0x0002);
}

TEST_F(HandlerTest, RunsProcessorOnEveryUpdate)
{
    handler.AddHandler(0x01, RecordingHandler, RecordingProcessor, nullptr);

    handler.Update();
    handler.Update();
    handler.Update();

    EXPECT_EQ(spy.processorCalls, 3u);
}

TEST_F(HandlerTest, DoesNotRegisterNullProcessor)
{
    handler.AddHandler(0x01, RecordingHandler, nullptr, nullptr);

    handler.Update();

    EXPECT_EQ(spy.processorCalls, 0u);
}

TEST_F(HandlerTest, SurvivesMoreProcessorsThanTableCapacity)
{
    for (int type = 0; type < 64; type++)
        handler.AddHandler(static_cast<uint8_t>(type), RecordingHandler, RecordingProcessor, nullptr);

    handler.Update();

    EXPECT_EQ(spy.processorCalls, 32u);
}

TEST_F(HandlerTest, SendPacketWritesBytesToChannel)
{
    std::vector<uint8_t> packet = PacketBuilder(0x02, 0x0009).Build();

    handler.SendPacket(packet.data(), static_cast<uint16_t>(packet.size()), 0x0009);

    ASSERT_EQ(channel.SentCount(), 1u);
    EXPECT_EQ(channel.LastSent().raw, packet);
    EXPECT_EQ(channel.LastSent().header.packetId, 0x0009);
}

TEST_F(HandlerTest, SendPacketDropsZeroPacketId)
{
    std::vector<uint8_t> packet = PacketBuilder(0x02, ZERO_PACKET_ID).Build();

    handler.SendPacket(packet.data(), static_cast<uint16_t>(packet.size()), ZERO_PACKET_ID);

    EXPECT_TRUE(channel.NothingSent());
}

TEST_F(HandlerTest, UpdateConsumesAllAvailableBytes)
{
    handler.AddHandler(0x01, RecordingHandler, nullptr, nullptr);

    channel.Feed(PacketBuilder(0x01, 0x0001).GarbagePrefix(5).Build());
    handler.Update();

    EXPECT_EQ(channel.PendingRx(), 0u);
    EXPECT_EQ(spy.handlerCalls, 1u);
}
