#include <gtest/gtest.h>
#include <string.h>
#include <vector>
#include "../../src/protocol/com_protocol_parser.h"

using ComLinkRTProtocol::PacketHeader;
using ComLinkRTProtocol::ProtocolParser;

namespace
{
    const size_t HEADER_SIZE = sizeof(PacketHeader);

    std::vector<uint8_t> Create(uint8_t version, uint8_t type, uint8_t serviceBits, uint16_t packetId,
                                const std::vector<uint8_t>& payload)
    {
        ProtocolParser parser;
        std::vector<uint8_t> buffer(HEADER_SIZE + payload.size() + 16, 0);
        uint16_t length = 0;

        parser.CreatePacket(version, type, serviceBits, packetId,
                            payload.empty() ? nullptr : payload.data(),
                            static_cast<uint16_t>(payload.size()),
                            buffer.data(), length);

        buffer.resize(length);
        return buffer;
    }

    uint16_t ReadLe16(const std::vector<uint8_t>& bytes, size_t offset)
    {
        return static_cast<uint16_t>(bytes[offset] | (bytes[offset + 1] << 8));
    }
}

TEST(CreatePacket, HeaderIsElevenBytes)
{
    EXPECT_EQ(HEADER_SIZE, 11u);
}

TEST(CreatePacket, WritesFieldsAtExpectedOffsets)
{
    std::vector<uint8_t> packet = Create(PROTOCOL_VERSION, 0x07, SERVICE_BIT_KEEP_ALIVE, 0xBEEF, {});

    EXPECT_EQ(packet[0], SYNC_BYTE1);
    EXPECT_EQ(packet[1], SYNC_BYTE2);
    EXPECT_EQ(packet[2], PROTOCOL_VERSION);
    EXPECT_EQ(packet[3], 0x07);
    EXPECT_EQ(packet[4], SERVICE_BIT_KEEP_ALIVE);
    EXPECT_EQ(ReadLe16(packet, 5), 0xBEEF);
    EXPECT_EQ(ReadLe16(packet, 7), 0u);
}

TEST(CreatePacket, OutputLengthIsHeaderPlusPayload)
{
    std::vector<uint8_t> payload(37, 0x5A);
    std::vector<uint8_t> packet = Create(PROTOCOL_VERSION, 0x01, EMPTY_SERVICE_BITS, 1, payload);

    EXPECT_EQ(packet.size(), HEADER_SIZE + payload.size());
    EXPECT_EQ(ReadLe16(packet, 7), payload.size());
}

TEST(CreatePacket, CopiesPayloadAfterHeader)
{
    std::vector<uint8_t> payload = {0xDE, 0xAD, 0xBE, 0xEF};
    std::vector<uint8_t> packet = Create(PROTOCOL_VERSION, 0x01, EMPTY_SERVICE_BITS, 1, payload);

    EXPECT_EQ(memcmp(packet.data() + HEADER_SIZE, payload.data(), payload.size()), 0);
}

TEST(CreatePacket, CrcCoversHeaderWithoutCrcFieldAndPayload)
{
    std::vector<uint8_t> payload = {0x11, 0x22, 0x33};
    std::vector<uint8_t> packet = Create(PROTOCOL_VERSION, 0x03, EMPTY_SERVICE_BITS, 0x0102, payload);

    uint16_t expected = ProtocolParser::CalculateCRC(packet.data(), HEADER_SIZE - sizeof(uint16_t));
    expected = ProtocolParser::UpdateCRC(expected, packet.data() + HEADER_SIZE, static_cast<uint16_t>(payload.size()));

    EXPECT_EQ(ReadLe16(packet, 9), expected);
}

TEST(CreatePacket, AcceptsNullPayloadPointer)
{
    std::vector<uint8_t> packet = Create(PROTOCOL_VERSION, 0x01, EMPTY_SERVICE_BITS, 1, {});

    EXPECT_EQ(packet.size(), HEADER_SIZE);
    EXPECT_EQ(ReadLe16(packet, 7), 0u);
}

TEST(CreatePacket, DifferentServiceBitsProduceDifferentCrc)
{
    std::vector<uint8_t> plain = Create(PROTOCOL_VERSION, 0x03, EMPTY_SERVICE_BITS, 7, {});
    std::vector<uint8_t> subscribed = Create(PROTOCOL_VERSION, 0x03, SERVICE_BIT_SUBSCRIBE, 7, {});

    EXPECT_NE(ReadLe16(plain, 9), ReadLe16(subscribed, 9));
}

TEST(CreatePacket, ProducedPacketIsAcceptedByParser)
{
    std::vector<uint8_t> payload = {0x01, 0x02, 0x03, 0x04, 0x05};
    std::vector<uint8_t> packet = Create(PROTOCOL_VERSION, 0x04, SERVICE_BIT_SUBSCRIBE, 0x1234, payload);

    ProtocolParser parser;
    bool complete = false;
    for (uint8_t byte : packet)
        complete = parser.ProcessByte(byte);

    ASSERT_TRUE(complete);
    EXPECT_EQ(parser.GetHeader().packetType, 0x04);
    EXPECT_EQ(parser.GetHeader().packetId, 0x1234);
    EXPECT_EQ(parser.GetHeader().serviceBits, SERVICE_BIT_SUBSCRIBE);
    EXPECT_EQ(parser.GetHeader().dataLength, payload.size());
    EXPECT_EQ(memcmp(parser.GetPayloadPtr(), payload.data(), payload.size()), 0);
}
