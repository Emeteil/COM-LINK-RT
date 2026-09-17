#pragma once
#include "com_protocol_channel.h"
#include "com_protocol_parser.h"

namespace ComLinkRTProtocol
{
    class ProtocolHandler
    {
        private:
        struct CommandSlot
        {
            PacketHandlerFn handler;
            PacketProcessorFn processor;
        };

        IProtocolChannel& channel;
        CommandSlot handlersTable[256];
        PacketProcessorFn processorsList[32];
        uint8_t processorsCount;

        void HandlePacket();

        public:
        ProtocolParser parser;

        explicit ProtocolHandler(IProtocolChannel& channel);

        void Begin();
        void AddHandler(uint8_t packetType, PacketHandlerFn handler, PacketProcessorFn processor, PacketInitFn initFunc);
        void Update();
        void SendPacket(const uint8_t* data, uint16_t length, uint16_t packetId) const;
    };
}
