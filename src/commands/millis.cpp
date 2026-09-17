#include "millis.h"
#include "../core/subscription_base.h"
#include "../core/globals.h"
#include <Arduino.h>

namespace CommandMillis
{
    class MillisHandler : public CommandSubscription::SubscriptionHandler
    {
        protected:
        void SendResponse(uint8_t packetType, uint16_t packetId, uint8_t serviceBits) override
        {
            uint8_t txBuffer[64];
            uint16_t length;

            uint32_t currentMillis = millis();

            protocol.parser.CreatePacket(
                PROTOCOL_VERSION,
                packetType,
                serviceBits,
                packetId,
                reinterpret_cast<uint8_t*>(&currentMillis),
                sizeof(currentMillis),
                txBuffer,
                length);

            protocol.SendPacket(txBuffer, length, packetId);
        }

        public:
        MillisHandler() : SubscriptionHandler(3000, 10000, PACKET_TYPE_RESPONSE) {}
    };

    MillisHandler millisHandler;

    void Handler(const ComLinkRTProtocol::PacketHeader& header, const uint8_t* data)
    {
        millisHandler.HandleSubscription(header, data);
    }

    void Processor()
    {
        millisHandler.ProcessSubscription();
    }
}