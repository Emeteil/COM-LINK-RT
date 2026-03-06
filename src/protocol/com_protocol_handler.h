#pragma once
#include "com_protocol_parser.h"
#include <vector>
#include <functional>
#include <unordered_map>

namespace ComLinkRTProtocol
{
    class ProtocolHandler
    {
        private:
        struct CommandHandler
        {
            uint8_t packetType;
            std::function<void(PacketHeader, uint8_t*)> handler;
            std::function<void()> processor;
        };
        std::unordered_map<uint8_t, CommandHandler> handlersTable;
        std::vector<uint8_t> packetTypes;

        void HandlePacket(const PacketHeader &header);

        public:
        ProtocolParser parser;

        ProtocolHandler();

        void Begin();

        void AddHandler(uint8_t packetType, std::function<void(PacketHeader, uint8_t*)> handler, std::function<void()> processor, std::function<void()> initFunc);

        void Update();

        void SendPacket(const uint8_t *data, uint16_t length, uint16_t packetId) const;
    };
}