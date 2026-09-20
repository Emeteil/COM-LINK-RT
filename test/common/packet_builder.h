#pragma once
#include <stdint.h>
#include <string.h>
#include <vector>
#include "../../src/protocol/com_protocol.h"
#include "../../src/protocol/com_protocol_parser.h"

namespace TestSupport
{
    static const size_t HEADER_SIZE = sizeof(ComLinkRTProtocol::PacketHeader);
    static const size_t OFFSET_VERSION = 2;
    static const size_t OFFSET_TYPE = 3;
    static const size_t OFFSET_SERVICE_BITS = 4;
    static const size_t OFFSET_PACKET_ID = 5;
    static const size_t OFFSET_DATA_LENGTH = 7;
    static const size_t OFFSET_CRC = 9;

    class PacketBuilder
    {
        public:
        explicit PacketBuilder(uint8_t packetType, uint16_t packetId = 0x0001)
            : type(packetType), id(packetId), version(PROTOCOL_VERSION), serviceBits(EMPTY_SERVICE_BITS),
              badCrc(false), hasDeclaredLength(false), declaredLength(0), garbagePrefix(0)
        {
        }

        PacketBuilder& Version(uint8_t value)
        {
            version = value;
            return *this;
        }

        PacketBuilder& ServiceBits(uint8_t value)
        {
            serviceBits = value;
            return *this;
        }

        PacketBuilder& Id(uint16_t value)
        {
            id = value;
            return *this;
        }

        PacketBuilder& Payload(const void* data, uint16_t length)
        {
            const uint8_t* bytes = static_cast<const uint8_t*>(data);
            payload.assign(bytes, bytes + length);
            return *this;
        }

        PacketBuilder& Payload(const std::vector<uint8_t>& bytes)
        {
            payload = bytes;
            return *this;
        }

        template <typename T>
        PacketBuilder& PayloadOf(const T& value)
        {
            return Payload(&value, static_cast<uint16_t>(sizeof(T)));
        }

        PacketBuilder& BreakCrc()
        {
            badCrc = true;
            return *this;
        }

        PacketBuilder& DeclaredLength(uint16_t value)
        {
            hasDeclaredLength = true;
            declaredLength = value;
            return *this;
        }

        PacketBuilder& GarbagePrefix(size_t count)
        {
            garbagePrefix = count;
            return *this;
        }

        std::vector<uint8_t> Build() const
        {
            std::vector<uint8_t> packet(HEADER_SIZE + payload.size());

            ComLinkRTProtocol::ProtocolParser builder;
            uint16_t length = 0;
            builder.CreatePacket(version, type, serviceBits, id,
                                 payload.empty() ? nullptr : payload.data(),
                                 static_cast<uint16_t>(payload.size()),
                                 packet.data(), length);
            packet.resize(length);

            if (hasDeclaredLength)
            {
                packet[OFFSET_DATA_LENGTH] = static_cast<uint8_t>(declaredLength & 0xFF);
                packet[OFFSET_DATA_LENGTH + 1] = static_cast<uint8_t>(declaredLength >> 8);
                WriteCrc(packet, ComputeCrc(packet));
            }

            if (badCrc)
                WriteCrc(packet, static_cast<uint16_t>(ComputeCrc(packet) ^ 0xFFFF));

            if (garbagePrefix > 0)
            {
                std::vector<uint8_t> prefixed;
                for (size_t i = 0; i < garbagePrefix; i++)
                    prefixed.push_back(static_cast<uint8_t>(0xA0 + i));
                prefixed.insert(prefixed.end(), packet.begin(), packet.end());
                return prefixed;
            }

            return packet;
        }

        operator std::vector<uint8_t>() const { return Build(); }

        private:
        static uint16_t ComputeCrc(const std::vector<uint8_t>& packet)
        {
            uint16_t crc = ComLinkRTProtocol::ProtocolParser::CalculateCRC(packet.data(), OFFSET_CRC);
            if (packet.size() > HEADER_SIZE)
                crc = ComLinkRTProtocol::ProtocolParser::UpdateCRC(crc, packet.data() + HEADER_SIZE,
                                                                   static_cast<uint16_t>(packet.size() - HEADER_SIZE));
            return crc;
        }

        static void WriteCrc(std::vector<uint8_t>& packet, uint16_t crc)
        {
            packet[OFFSET_CRC] = static_cast<uint8_t>(crc & 0xFF);
            packet[OFFSET_CRC + 1] = static_cast<uint8_t>(crc >> 8);
        }

        uint8_t type;
        uint16_t id;
        uint8_t version;
        uint8_t serviceBits;
        std::vector<uint8_t> payload;
        bool badCrc;
        bool hasDeclaredLength;
        uint16_t declaredLength;
        size_t garbagePrefix;
    };
}
