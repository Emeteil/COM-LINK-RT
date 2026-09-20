#pragma once
#include <gtest/gtest.h>
#include <vector>
#include "arduino/mock_state.h"
#include "fake_channel.h"
#include "packet_builder.h"
#include "test_globals.h"

namespace TestSupport
{
    inline void ExpectPacket(const SentPacket& packet, uint8_t type, uint16_t id, uint8_t serviceBits = EMPTY_SERVICE_BITS)
    {
        EXPECT_EQ(packet.header.syncByte1, SYNC_BYTE1);
        EXPECT_EQ(packet.header.syncByte2, SYNC_BYTE2);
        EXPECT_EQ(packet.header.version, PROTOCOL_VERSION);
        EXPECT_EQ(packet.header.packetType, type);
        EXPECT_EQ(packet.header.packetId, id);
        EXPECT_EQ(packet.header.serviceBits, serviceBits);
        EXPECT_EQ(packet.header.dataLength, packet.payload.size());
    }

    class ProtocolFixture : public ::testing::Test
    {
        protected:
        FakeChannel& channel = Channel();

        void SetUp() override
        {
            MockArduino::ResetAll();
            channel.Clear();
            protocol.parser.Reset();
        }

        void Feed(const std::vector<uint8_t>& bytes) { channel.Feed(bytes); }

        void Feed(const PacketBuilder& packet) { channel.Feed(packet.Build()); }

        void Advance(unsigned long ms) { MockArduino::Advance(ms); }

        void Tick(unsigned long ms = 0)
        {
            if (ms > 0)
                Advance(ms);
            protocol.Update();
        }

        void Exchange(const PacketBuilder& packet)
        {
            Feed(packet);
            Tick();
        }

        const SentPacket& LastSent() const { return channel.LastSent(); }

        size_t SentCount() const { return channel.SentCount(); }
    };
}
