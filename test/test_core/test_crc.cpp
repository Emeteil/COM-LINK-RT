#include <gtest/gtest.h>
#include <string.h>
#include <vector>
#include "../../src/protocol/com_protocol_parser.h"

using ComLinkRTProtocol::ProtocolParser;

namespace
{
    std::vector<uint8_t> Bytes(const char* text)
    {
        return std::vector<uint8_t>(text, text + strlen(text));
    }
}

TEST(Crc, EmptyInputReturnsSeed)
{
    EXPECT_EQ(ProtocolParser::CalculateCRC(nullptr, 0), 0xFFFF);
}

TEST(Crc, MatchesCcittFalseReferenceVector)
{
    std::vector<uint8_t> data = Bytes("123456789");
    EXPECT_EQ(ProtocolParser::CalculateCRC(data.data(), static_cast<uint16_t>(data.size())), 0x29B1);
}

TEST(Crc, IncrementalUpdateEqualsSingleShot)
{
    std::vector<uint8_t> data = Bytes("com-link-rt protocol payload");

    uint16_t whole = ProtocolParser::CalculateCRC(data.data(), static_cast<uint16_t>(data.size()));

    uint16_t split = ProtocolParser::CalculateCRC(data.data(), 9);
    split = ProtocolParser::UpdateCRC(split, data.data() + 9, static_cast<uint16_t>(data.size() - 9));

    EXPECT_EQ(split, whole);
}

TEST(Crc, ByteByByteUpdateEqualsSingleShot)
{
    std::vector<uint8_t> data = Bytes("abcdefghijklmnop");

    uint16_t crc = 0xFFFF;
    for (uint8_t byte : data)
        crc = ProtocolParser::UpdateCRC(crc, &byte, 1);

    EXPECT_EQ(crc, ProtocolParser::CalculateCRC(data.data(), static_cast<uint16_t>(data.size())));
}

TEST(Crc, SingleBitFlipChangesResult)
{
    std::vector<uint8_t> data = Bytes("payload");
    uint16_t original = ProtocolParser::CalculateCRC(data.data(), static_cast<uint16_t>(data.size()));

    data[3] ^= 0x01;
    uint16_t modified = ProtocolParser::CalculateCRC(data.data(), static_cast<uint16_t>(data.size()));

    EXPECT_NE(original, modified);
}

TEST(Crc, OrderOfBytesMatters)
{
    std::vector<uint8_t> forward = {0x01, 0x02, 0x03, 0x04};
    std::vector<uint8_t> backward = {0x04, 0x03, 0x02, 0x01};

    EXPECT_NE(ProtocolParser::CalculateCRC(forward.data(), 4), ProtocolParser::CalculateCRC(backward.data(), 4));
}
