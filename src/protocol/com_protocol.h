#pragma once
#include <stdint.h>
#include "../core/board_config.h"

#define READING_SPEED SERIAL_BAUD_RATE

#define BUFFER_SIZE        256
#define SYNC_BYTE1         0xAA
#define SYNC_BYTE2         0x55
#define ZERO_PACKET_ID     0x0000
#define EMPTY_SERVICE_BITS 0x00
#define PROTOCOL_VERSION   0x03

#define SERVICE_BIT_SUBSCRIBE    0x80
#define SERVICE_BIT_UNSUBSCRIBED 0x40
#define SERVICE_BIT_KEEP_ALIVE   0x20
#define SERVICE_BIT_UNSUBSCRIBE  0x10

namespace ComLinkRTProtocol
{
#pragma pack(push, 1)
    struct PacketHeader
    {
        uint8_t syncByte1;   // 0xAA - первый синхробайт
        uint8_t syncByte2;   // 0x55 - второй синхробайт
        uint8_t version;     // Версия протокола
        uint8_t packetType;  // Тип пакета
        uint8_t serviceBits; // Служебные биты
        uint16_t packetId;   // Идентификатор пакета
        uint16_t dataLength; // Длина данных
        uint16_t crc;        // Контрольная сумма заголовка
    };
#pragma pack(pop)

#pragma pack(push, 1)
    struct _Bits
    {
        uint8_t b0 : 1;
        uint8_t b1 : 1;
        uint8_t b2 : 1;
        uint8_t b3 : 1;
        uint8_t b4 : 1;
        uint8_t b5 : 1;
        uint8_t b6 : 1;
        uint8_t b7 : 1;
    };
#pragma pack(pop)

#pragma pack(push, 1)
    struct _ServiceBits
    {
        uint8_t subscribe : 1;
        uint8_t unsubscribed : 1;
        uint8_t keepAlive : 1;
        uint8_t unsubscribe : 1;
        uint8_t otherBits : 4;
    };
#pragma pack(pop)

#pragma pack(push, 1)
    union ServiceBits
    {
        _Bits bits;
        _ServiceBits fields;
        uint8_t byte;

        ServiceBits() : byte(0) {}
        ServiceBits(uint8_t value) : byte(value) {}

        bool operator==(const ServiceBits& other) const { return byte == other.byte; }
        bool operator==(uint8_t value) const { return byte == value; }
    };
#pragma pack(pop)

    typedef void (*PacketHandlerFn)(const PacketHeader&, const uint8_t*);
    typedef void (*PacketProcessorFn)();
    typedef void (*PacketInitFn)();
}
