#include "com_protocol_handler.h"
#include <Arduino.h>
#include <string.h>

namespace ComLinkRTProtocol
{
    ProtocolHandler::ProtocolHandler() : processorsCount(0)
    {
        memset(handlersTable, 0, sizeof(handlersTable));
    }

    void ProtocolHandler::Begin()
    {
        Serial1.begin(READING_SPEED);
        parser.Reset();
    }

    void ProtocolHandler::AddHandler(uint8_t packetType, PacketHandlerFn handler, PacketProcessorFn processor, PacketInitFn initFunc)
    {
        if (initFunc != nullptr)
            initFunc();

        handlersTable[packetType].handler = handler;
        handlersTable[packetType].processor = processor;

        if (processor != nullptr && processorsCount < (sizeof(processorsList) / sizeof(processorsList[0])))
        {
            processorsList[processorsCount++] = processor;
        }
    }

    void ProtocolHandler::Update()
    {
        int available = Serial1.available();
        while (available-- > 0)
        {
            uint8_t b = static_cast<uint8_t>(Serial1.read());
            if (parser.ProcessByte(b))
                HandlePacket();
        }

        const uint8_t count = processorsCount;
        for (uint8_t i = 0; i < count; i++)
            processorsList[i]();
    }

    void ProtocolHandler::HandlePacket()
    {
        const PacketHeader& header = parser.GetHeader();

        if (header.version != PROTOCOL_VERSION)
        {
            parser.Reset();
            return;
        }

        PacketHandlerFn fn = handlersTable[header.packetType].handler;
        if (fn == nullptr)
        {
            parser.Reset();
            return;
        }

        const uint8_t* data = (header.dataLength > 0) ? parser.GetPayloadPtr() : nullptr;
        fn(header, data);

        parser.Reset();
    }

    void ProtocolHandler::SendPacket(const uint8_t* data, uint16_t length, uint16_t packetId) const
    {
        if (packetId == ZERO_PACKET_ID)
            return;

        Serial1.write(data, length);
    }
}