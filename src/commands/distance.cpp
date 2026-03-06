#include "distance.h"
#include "../core/subscription_base.h"
#include "../core/globals.h"
#include <Arduino.h>

namespace CommandDistance
{
    class DistanceHandler : public CommandSubscription::SubscriptionHandler
    {
        protected:
        void SendResponse(uint8_t packetType, uint16_t packetId, uint8_t serviceBits) override
        {
            uint8_t txBuffer[64];
            uint16_t length;
            
            digitalWrite(TRIG_PIN, LOW);
            delayMicroseconds(5);
            digitalWrite(TRIG_PIN, HIGH);
            
            delayMicroseconds(10);
            digitalWrite(TRIG_PIN, LOW);

            long duration = pulseIn(ECHO_PIN, HIGH);
            long cm = (duration / 2) / 29.1;
            
            protocol.parser.CreatePacket(
                PROTOCOL_VERSION,
                packetType,
                serviceBits,
                packetId,
                reinterpret_cast<uint8_t*>(&cm),
                sizeof(cm),
                txBuffer,
                length
            );
            
            protocol.SendPacket(txBuffer, length, packetId);
        }
        
        public:
        DistanceHandler() : SubscriptionHandler(100, 10000, PACKET_TYPE_RESPONSE) {}
    };

    DistanceHandler distanceHandler;

    void Init()
    {
        pinMode(TRIG_PIN, OUTPUT);
        pinMode(ECHO_PIN, INPUT);
    }

    void Handler(ComLinkRTProtocol::PacketHeader header, uint8_t* data)
    {
        distanceHandler.HandleSubscription(header, data);
    }
    
    void Processor()
    {
        distanceHandler.ProcessSubscription();
    }
}