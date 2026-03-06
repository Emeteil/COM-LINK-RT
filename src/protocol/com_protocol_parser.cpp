#include "com_protocol_parser.h"
#include <string.h>

namespace ComLinkRTProtocol
{
    ProtocolParser::ProtocolParser()
    {
        Reset();
    }
    
    PacketHeader ProtocolParser::GetHeader() const { return currentHeader; }

    ProtocolParser::State ProtocolParser::GetState() const { return state; }

    void ProtocolParser::Reset()
    {
        state = State::SYNC1;
        bufferIndex = 0;
        memset(&currentHeader, 0, sizeof(currentHeader));
        memset(buffer, 0, sizeof(buffer));
    }

    uint16_t ProtocolParser::CalculateCRC(const uint8_t *data, uint16_t length)
    {
        uint16_t crc = 0xFFFF;
        for (uint16_t i = 0; i < length; i++)
        {
            crc ^= (uint16_t)data[i] << 8;
            for (uint8_t j = 0; j < 8; j++)
            {
                if (crc & 0x8000)
                    crc = (crc << 1) ^ 0x1021;
                else
                    crc <<= 1;
            }
        }
        return crc;
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
                else Reset(); // Синхронизация нарушена
                break;

            case State::HEADER:
                buffer[bufferIndex++] = byte;

                if (bufferIndex >= sizeof(PacketHeader))
                {
                    memcpy(&currentHeader, buffer, sizeof(PacketHeader));

                    uint16_t receivedCRC = currentHeader.crc;
                    uint16_t calculatedCRC = CalculateCRC(buffer, sizeof(PacketHeader) - sizeof(PacketHeader::crc));

                    if (receivedCRC == calculatedCRC && currentHeader.dataLength <= sizeof(buffer) - sizeof(PacketHeader))
                    {
                        if (currentHeader.dataLength > 0)
                        {
                            state = State::PAYLOAD;
                        }
                        else
                        {
                            state = State::COMPLETE;
                            return true;
                        }
                    }
                    else Reset(); // Ошибка CRC или слишком большой пакет
                }
                break;

            case State::PAYLOAD:
                buffer[bufferIndex++] = byte;

                if (bufferIndex >= sizeof(PacketHeader) + currentHeader.dataLength)
                {
                    state = State::COMPLETE;
                    return true;
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
    
    bool ProtocolParser::GetPayload(uint8_t* data, uint16_t length)
    {
        if (state == State::COMPLETE && length == currentHeader.dataLength)
        {
            memcpy(data, buffer + sizeof(PacketHeader), length);
            Reset();
            return true;
        }
        return false;
    }

    void ProtocolParser::CreatePacket(uint8_t version, uint8_t type, uint8_t serviceBits, uint16_t packetId, uint8_t* data, uint16_t dataLength,
                                      uint8_t *outputBuffer, uint16_t &outputLength) const
    {
        PacketHeader header;
        header.syncByte1 = SYNC_BYTE1;
        header.syncByte2 = SYNC_BYTE2;
        header.version = version;
        header.packetType = type;
        header.serviceBits = serviceBits;
        header.packetId = packetId;
        header.dataLength = dataLength;
        header.crc = 0;

        header.crc = CalculateCRC(reinterpret_cast<const uint8_t *>(&header), sizeof(header) - sizeof(header.crc));

        memcpy(outputBuffer, &header, sizeof(header));
        outputLength = sizeof(header);

        if (dataLength > 0 && data != nullptr)
        {
            memcpy(outputBuffer + sizeof(header), data, dataLength);
            outputLength += dataLength;
        }
    }
}