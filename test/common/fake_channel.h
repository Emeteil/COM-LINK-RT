#pragma once
#include <stdint.h>
#include <string.h>
#include <deque>
#include <vector>
#include "../../src/protocol/com_protocol.h"
#include "../../src/protocol/com_protocol_channel.h"

namespace TestSupport
{
    struct SentPacket
    {
        ComLinkRTProtocol::PacketHeader header;
        std::vector<uint8_t> payload;
        std::vector<uint8_t> raw;

        template <typename T>
        T PayloadAs() const
        {
            T value;
            memset(&value, 0, sizeof(T));
            if (payload.size() >= sizeof(T))
                memcpy(&value, payload.data(), sizeof(T));
            return value;
        }
    };

    class FakeChannel : public ComLinkRTProtocol::IProtocolChannel
    {
        public:
        FakeChannel() : beginCount(0) {}

        void Begin() override { beginCount++; }

        int Available() override { return static_cast<int>(rx.size()); }

        uint8_t Read() override
        {
            if (rx.empty())
                return 0;

            uint8_t byte = rx.front();
            rx.pop_front();
            return byte;
        }

        void Write(const uint8_t* data, uint16_t length) override
        {
            std::vector<uint8_t> raw(data, data + length);
            txBytes.insert(txBytes.end(), raw.begin(), raw.end());

            SentPacket packet;
            memset(&packet.header, 0, sizeof(packet.header));
            packet.raw = raw;

            if (length >= sizeof(ComLinkRTProtocol::PacketHeader))
            {
                memcpy(&packet.header, data, sizeof(ComLinkRTProtocol::PacketHeader));
                packet.payload.assign(data + sizeof(ComLinkRTProtocol::PacketHeader), data + length);
            }

            sent.push_back(packet);
        }

        void Feed(const std::vector<uint8_t>& bytes) { rx.insert(rx.end(), bytes.begin(), bytes.end()); }

        void Feed(const uint8_t* bytes, size_t length) { rx.insert(rx.end(), bytes, bytes + length); }

        void FeedByte(uint8_t byte) { rx.push_back(byte); }

        void Clear()
        {
            rx.clear();
            sent.clear();
            txBytes.clear();
        }

        size_t SentCount() const { return sent.size(); }

        bool NothingSent() const { return sent.empty(); }

        const SentPacket& Sent(size_t index) const { return sent.at(index); }

        const SentPacket& LastSent() const { return sent.back(); }

        const std::vector<uint8_t>& TxBytes() const { return txBytes; }

        size_t PendingRx() const { return rx.size(); }

        unsigned int BeginCount() const { return beginCount; }

        private:
        std::deque<uint8_t> rx;
        std::vector<SentPacket> sent;
        std::vector<uint8_t> txBytes;
        unsigned int beginCount;
    };
}
