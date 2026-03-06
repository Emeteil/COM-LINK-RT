#include "com_protocol_handler.h"
#include "com_protocol.h"
#include <Arduino.h>

namespace ComLinkRTProtocol
{
    ProtocolHandler::ProtocolHandler() {}

    void ProtocolHandler::Begin()
    {
        Serial1.begin(READING_SPEED);
        parser.Reset();
    }

    void ProtocolHandler::AddHandler(uint8_t packetType, std::function<void(PacketHeader, uint8_t*)> handler, std::function<void()> processor, std::function<void()> initFunc)
    {
        if (initFunc != nullptr)
            initFunc();

        handlersTable[packetType] = {packetType, handler, processor};
        packetTypes.push_back(packetType);
    }

    void ProtocolHandler::Update()
    {
        while (Serial1.available() > 0)
        {
            uint8_t receivedByte = Serial1.read();

            if (parser.ProcessByte(receivedByte))
            {
                PacketHeader header = parser.GetHeader();
                HandlePacket(header);
            }
        }

        for (uint8_t packetType : packetTypes)
        {
            CommandHandler handler;
            handler = handlersTable[packetType];
            if (handler.processor == nullptr) continue;
            handler.processor();
        }
    }

    void ProtocolHandler::HandlePacket(const PacketHeader &header)
    {
        if (header.version != PROTOCOL_VERSION)
        {
            parser.Reset();
            return;
        }

        bool status = handlersTable.find(header.packetType) != handlersTable.end();
        if (!status) 
        {
            parser.Reset();
            return;
        }

        CommandHandler handler = handlersTable[header.packetType];

        uint8_t* data = nullptr;

        if (header.dataLength > 0)
        {
            data = new uint8_t[header.dataLength];
            if (!parser.GetPayload(data, header.dataLength))
            {
                delete[] data;
                parser.Reset();
                return;
            }
        }
        
        handler.handler(header, data);

        if (data != nullptr)
            delete[] data;
        
        parser.Reset();
    }
        
    void ProtocolHandler::SendPacket(const uint8_t *data, uint16_t length, uint16_t packetId) const
    {
        if (packetId == ZERO_PACKET_ID)
            return;
        
        Serial1.write(data, length);
    }
}