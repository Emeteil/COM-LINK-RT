#include "com_protocol_parser.h"
#include <string.h>

namespace ComLinkRTProtocol
{
    namespace
    {
        struct CrcTable
        {
            uint16_t t[256];
            constexpr CrcTable() : t()
            {
                for (int i = 0; i < 256; i++)
                {
                    uint16_t c = static_cast<uint16_t>(i) << 8;
                    for (int j = 0; j < 8; j++)
                        c = (c & 0x8000) ? static_cast<uint16_t>((c << 1) ^ 0x1021) : static_cast<uint16_t>(c << 1);
                    t[i] = c;
                }
            }
        };
        constexpr CrcTable CRC_TBL{};
    }

    ProtocolParser::ProtocolParser()
    {
        Reset();
    }

    void ProtocolParser::Reset()
    {
        state = State::SYNC1;
        bufferIndex = 0;
        expectedTotal = 0;
    }

    uint16_t ProtocolParser::UpdateCRC(uint16_t crc, const uint8_t* data, uint16_t length)
    {
        for (uint16_t i = 0; i < length; i++)
            crc = static_cast<uint16_t>((crc << 8) ^ CRC_TBL.t[((crc >> 8) ^ data[i]) & 0xFF]);
        return crc;
    }

    uint16_t ProtocolParser::CalculateCRC(const uint8_t* data, uint16_t length)
    {
        return UpdateCRC(0xFFFF, data, length);
    }

    bool ProtocolParser::ProcessByte(uint8_t byte)
    {
        switch (state)
        {
            case State::SYNC1:
                if (byte == SYNC_BYTE1)
                {
                    bufferIndex = 0;
                    buffer[bufferIndex++] = byte;
                    state = State::SYNC2;
                }
                break;

            case State::SYNC2:
                if (byte == SYNC_BYTE2)
                {
                    buffer[bufferIndex++] = byte;
                    state = State::HEADER;
                }
                else
                    Reset(); // Синхронизация нарушена
                break;

            case State::HEADER:
                buffer[bufferIndex++] = byte;
                if (bufferIndex >= sizeof(PacketHeader))
                {
                    memcpy(&currentHeader, buffer, sizeof(PacketHeader));

                    if (currentHeader.dataLength > BUFFER_SIZE - sizeof(PacketHeader))
                    {
                        Reset();
                        break;
                    }

                    expectedTotal = sizeof(PacketHeader) + currentHeader.dataLength;

                    if (currentHeader.dataLength == 0)
                    {
                        uint16_t headerNoCrc = sizeof(PacketHeader) - sizeof(uint16_t);
                        uint16_t crc = CalculateCRC(buffer, headerNoCrc);
                        if (crc == currentHeader.crc)
                        {
                            state = State::COMPLETE;
                            return true;
                        }
                        Reset();
                    }
                    else
                    {
                        state = State::PAYLOAD;
                    }
                }
                break;

            case State::PAYLOAD:
                buffer[bufferIndex++] = byte;
                if (bufferIndex >= expectedTotal)
                {
                    uint16_t headerNoCrc = sizeof(PacketHeader) - sizeof(uint16_t);
                    uint16_t crc = CalculateCRC(buffer, headerNoCrc);
                    crc = UpdateCRC(crc, buffer + sizeof(PacketHeader), currentHeader.dataLength);
                    if (crc == currentHeader.crc)
                    {
                        state = State::COMPLETE;
                        return true;
                    }
                    Reset();
                }
                break;

            case State::COMPLETE:
                // Ожидаем Reset через GetPayload()
                break;

            default:
                Reset();
                break;
        }

        return false;
    }

    void ProtocolParser::CreatePacket(uint8_t version, uint8_t type, uint8_t serviceBits, uint16_t packetId,
                                      const uint8_t* data, uint16_t dataLength,
                                      uint8_t* outputBuffer, uint16_t& outputLength) const
    {
        PacketHeader* header = reinterpret_cast<PacketHeader*>(outputBuffer);
        header->syncByte1 = SYNC_BYTE1;
        header->syncByte2 = SYNC_BYTE2;
        header->version = version;
        header->packetType = type;
        header->serviceBits = serviceBits;
        header->packetId = packetId;
        header->dataLength = dataLength;
        header->crc = 0;

        if (dataLength > 0 && data != nullptr)
            memcpy(outputBuffer + sizeof(PacketHeader), data, dataLength);

        uint16_t headerNoCrc = sizeof(PacketHeader) - sizeof(uint16_t);
        uint16_t crc = CalculateCRC(outputBuffer, headerNoCrc);
        if (dataLength > 0)
            crc = UpdateCRC(crc, outputBuffer + sizeof(PacketHeader), dataLength);

        header->crc = crc;

        outputLength = sizeof(PacketHeader) + dataLength;
    }
}
