#pragma once
#include <Arduino.h>
#include "../protocol/com_protocol.h"

namespace CommandSubscription
{
    struct SubscriptionData
    {
        bool isActive;
        uint16_t subscribedPacketId;
        uint8_t subscribedPacketType;
        unsigned long lastKeepAliveTime;
        unsigned long lastSendTime;
        bool pendingUnsubscribe;
    };

    class SubscriptionHandler
    {
        protected:
        SubscriptionData subscription;
        unsigned long sendInterval;
        unsigned long keepAliveTimeout;
        uint8_t responsePacketType;

        virtual void SendResponse(uint8_t packetType, uint16_t packetId, uint8_t serviceBits) = 0;
        uint8_t СombineHigh4Low4Bits(uint8_t high4_source, uint8_t low4_source);
        
        public:
        SubscriptionHandler(unsigned long interval, unsigned long timeout, uint8_t responseType);
        
        void ResetSubscription();
        void HandleSubscription(const ComLinkRTProtocol::PacketHeader& header, const uint8_t* data);
        void ProcessSubscription();
        bool IsActive() const { return subscription.isActive; }
    };
}