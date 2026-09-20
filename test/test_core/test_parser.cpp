#include <gtest/gtest.h>
#include <string.h>
#include <vector>
#include "../../src/protocol/com_protocol_parser.h"
#include "packet_builder.h"

using ComLinkRTProtocol::ProtocolParser;
using TestSupport::PacketBuilder;

namespace
{
    const size_t HEADER_SIZE = sizeof(ComLinkRTProtocol::PacketHeader);
    const size_t MAX_PAYLOAD = BUFFER_SIZE - HEADER_SIZE;

    class ParserTest : public ::testing::Test
    {
        protected:
        ProtocolParser parser;

        size_t FeedCountingPackets(const std::vector<uint8_t>& bytes)
        {
            size_t completed = 0;
            for (uint8_t byte : bytes)
            {
                if (parser.ProcessByte(byte))
                {
                    completed++;
                    parser.Reset();
                }
            }
            return completed;
        }

        bool FeedOne(const std::vector<uint8_t>& bytes)
        {
            bool complete = false;
            for (uint8_t byte : bytes)
                complete = parser.ProcessByte(byte) || complete;
            return complete;
        }
    };
}

TEST_F(ParserTest, StartsInSync1State)
{
    EXPECT_EQ(parser.GetState(), ProtocolParser::State::SYNC1);
}

TEST_F(ParserTest, AcceptsPacketWithoutPayload)
{
    ASSERT_TRUE(FeedOne(PacketBuilder(0x01, 0x0042).Build()));

    EXPECT_EQ(parser.GetState(), ProtocolParser::State::COMPLETE);
    EXPECT_EQ(parser.GetHeader().packetType, 0x01);
    EXPECT_EQ(parser.GetHeader().packetId, 0x0042);
    EXPECT_EQ(parser.GetHeader().dataLength, 0u);
}

TEST_F(ParserTest, AcceptsPacketWithPayload)
{
    std::vector<uint8_t> payload = {0x10, 0x20, 0x30, 0x40};

    ASSERT_TRUE(FeedOne(PacketBuilder(0x07, 0x0001).Payload(payload).Build()));

    EXPECT_EQ(parser.GetHeader().dataLength, payload.size());
    EXPECT_EQ(memcmp(parser.GetPayloadPtr(), payload.data(), payload.size()), 0);
}

TEST_F(ParserTest, SkipsGarbageBeforeSyncBytes)
{
    std::vector<uint8_t> stream = {0x00, 0x11, 0x22, 0x55, 0x33};
    std::vector<uint8_t> packet = PacketBuilder(0x01, 0x0007).Build();
    stream.insert(stream.end(), packet.begin(), packet.end());

    EXPECT_EQ(FeedCountingPackets(stream), 1u);
}

TEST_F(ParserTest, RejectsBrokenCrc)
{
    EXPECT_FALSE(FeedOne(PacketBuilder(0x01, 0x0001).BreakCrc().Build()));
    EXPECT_EQ(parser.GetState(), ProtocolParser::State::SYNC1);
}

TEST_F(ParserTest, RejectsBrokenCrcWithPayload)
{
    std::vector<uint8_t> payload = {0xAA, 0xBB};

    EXPECT_FALSE(FeedOne(PacketBuilder(0x01, 0x0001).Payload(payload).BreakCrc().Build()));
    EXPECT_EQ(parser.GetState(), ProtocolParser::State::SYNC1);
}

TEST_F(ParserTest, RecoversAfterBrokenPacket)
{
    std::vector<uint8_t> stream = PacketBuilder(0x01, 0x0001).BreakCrc().Build();
    std::vector<uint8_t> good = PacketBuilder(0x02, 0x0002).Build();
    stream.insert(stream.end(), good.begin(), good.end());

    EXPECT_EQ(FeedCountingPackets(stream), 1u);
    EXPECT_EQ(parser.GetHeader().packetId, 0x0002);
}

TEST_F(ParserTest, RejectsDeclaredLengthLargerThanBuffer)
{
    std::vector<uint8_t> packet = PacketBuilder(0x01, 0x0001).DeclaredLength(BUFFER_SIZE).Build();

    EXPECT_FALSE(FeedOne(packet));
    EXPECT_EQ(parser.GetState(), ProtocolParser::State::SYNC1);
}

TEST_F(ParserTest, RejectsDeclaredLengthJustOverLimit)
{
    std::vector<uint8_t> packet = PacketBuilder(0x01, 0x0001).DeclaredLength(static_cast<uint16_t>(MAX_PAYLOAD + 1)).Build();

    EXPECT_FALSE(FeedOne(packet));
    EXPECT_EQ(parser.GetState(), ProtocolParser::State::SYNC1);
}

TEST_F(ParserTest, AcceptsLargestPossiblePayload)
{
    std::vector<uint8_t> payload(MAX_PAYLOAD, 0x7E);

    ASSERT_TRUE(FeedOne(PacketBuilder(0x01, 0x0001).Payload(payload).Build()));
    EXPECT_EQ(parser.GetHeader().dataLength, MAX_PAYLOAD);
    EXPECT_EQ(memcmp(parser.GetPayloadPtr(), payload.data(), payload.size()), 0);
}

TEST_F(ParserTest, ParsesTwoPacketsInARow)
{
    std::vector<uint8_t> stream = PacketBuilder(0x01, 0x0001).Build();
    std::vector<uint8_t> second = PacketBuilder(0x03, 0x0002).Payload({0x01, 0x02}).Build();
    stream.insert(stream.end(), second.begin(), second.end());

    EXPECT_EQ(FeedCountingPackets(stream), 2u);
    EXPECT_EQ(parser.GetHeader().packetType, 0x03);
}

TEST_F(ParserTest, ToleratesGarbageBetweenPackets)
{
    std::vector<uint8_t> stream = PacketBuilder(0x01, 0x0001).Build();
    stream.push_back(0x00);
    stream.push_back(0x55);
    std::vector<uint8_t> second = PacketBuilder(0x01, 0x0002).Build();
    stream.insert(stream.end(), second.begin(), second.end());

    EXPECT_EQ(FeedCountingPackets(stream), 2u);
}

TEST_F(ParserTest, StaysCompleteUntilReset)
{
    std::vector<uint8_t> packet = PacketBuilder(0x01, 0x0001).Build();
    ASSERT_TRUE(FeedOne(packet));

    EXPECT_FALSE(parser.ProcessByte(SYNC_BYTE1));
    EXPECT_EQ(parser.GetState(), ProtocolParser::State::COMPLETE);
    EXPECT_EQ(parser.GetHeader().packetId, 0x0001);
}

TEST_F(ParserTest, ResetReturnsToSync1)
{
    ASSERT_TRUE(FeedOne(PacketBuilder(0x01, 0x0001).Build()));

    parser.Reset();

    EXPECT_EQ(parser.GetState(), ProtocolParser::State::SYNC1);
}

TEST_F(ParserTest, IgnoresWrongSecondSyncByte)
{
    parser.ProcessByte(SYNC_BYTE1);

    EXPECT_FALSE(parser.ProcessByte(0x00));
    EXPECT_EQ(parser.GetState(), ProtocolParser::State::SYNC1);
}

TEST_F(ParserTest, KeepsHeaderFieldsOfAcceptedPacket)
{
    ASSERT_TRUE(FeedOne(PacketBuilder(0x03, 0xABCD).ServiceBits(SERVICE_BIT_SUBSCRIBE).Build()));

    const ComLinkRTProtocol::PacketHeader& header = parser.GetHeader();
    EXPECT_EQ(header.syncByte1, SYNC_BYTE1);
    EXPECT_EQ(header.syncByte2, SYNC_BYTE2);
    EXPECT_EQ(header.version, PROTOCOL_VERSION);
    EXPECT_EQ(header.packetType, 0x03);
    EXPECT_EQ(header.serviceBits, SERVICE_BIT_SUBSCRIBE);
    EXPECT_EQ(header.packetId, 0xABCD);
}

TEST_F(ParserTest, AcceptsPacketWithForeignVersion)
{
    ASSERT_TRUE(FeedOne(PacketBuilder(0x01, 0x0001).Version(0x01).Build()));

    EXPECT_EQ(parser.GetHeader().version, 0x01);
}
