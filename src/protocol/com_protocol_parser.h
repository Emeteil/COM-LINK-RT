#pragma once
#include <stdint.h>
#include "com_protocol.h"

namespace ComLinkRTProtocol
{
    class ProtocolParser
    {
        public:
        enum class State
        {
            SYNC1,   // Ожидаем первый синхробайт
            SYNC2,   // Ожидаем второй синхробайт
            HEADER,  // Читаем заголовок
            PAYLOAD, // Читаем данные
            COMPLETE // Пакет полностью принят
        };

        private:
        State state;
        uint8_t buffer[BUFFER_SIZE];
        uint16_t bufferIndex;
        PacketHeader currentHeader;

        public:
        ProtocolParser();

        PacketHeader GetHeader() const;

        State GetState() const;

        void Reset();

        bool ProcessByte(uint8_t byte);

        bool GetPayload(uint8_t* data, uint16_t length);

        void CreatePacket(uint8_t version, uint8_t type, uint8_t serviceBits, uint16_t packetId, uint8_t* data, uint16_t dataLength,
                          uint8_t *outputBuffer, uint16_t &outputLength) const;
        
        static uint16_t CalculateCRC(const uint8_t *data, uint16_t length);
    };
}