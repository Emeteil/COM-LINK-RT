#include "subscription_base.h"
#include "globals.h"

namespace CommandSubscription
{
    SubscriptionHandler::SubscriptionHandler(unsigned long interval, unsigned long timeout, uint8_t responseType) : sendInterval(interval), keepAliveTimeout(timeout), responsePacketType(responseType)
    {
        ResetSubscription();
    }

    void SubscriptionHandler::ResetSubscription()
    {
        subscription.isActive = false;
        subscription.subscribedPacketId = 0;
        subscription.subscribedPacketType = 0;
        subscription.lastKeepAliveTime = 0;
        subscription.lastSendTime = 0;
        subscription.pendingUnsubscribe = false;
    }

    uint8_t SubscriptionHandler::СombineHigh4Low4Bits(uint8_t high4_source, uint8_t low4_source)
    {
        return (high4_source & 0xF0) | (low4_source & 0x0F);
    }

    void SubscriptionHandler::HandleSubscription(const ComLinkRTProtocol::PacketHeader& header, const uint8_t* data)
    {
        if (header.serviceBits & SERVICE_BIT_SUBSCRIBE && header.packetId != ZERO_PACKET_ID)
        {
            if (subscription.isActive)
                return;
            
            subscription.isActive = true;
            subscription.subscribedPacketId = header.packetId;
            subscription.subscribedPacketType = responsePacketType;
            subscription.lastKeepAliveTime = millis();
            subscription.lastSendTime = millis();
            subscription.pendingUnsubscribe = false;
            
            SendResponse(responsePacketType, header.packetId, СombineHigh4Low4Bits(EMPTY_SERVICE_BITS, header.serviceBits));
            return;
        }
        
        if (subscription.isActive && (header.serviceBits & SERVICE_BIT_KEEP_ALIVE))
        {
            if (header.packetId == subscription.subscribedPacketId)
                subscription.lastKeepAliveTime = millis();
            
            return;
        }
        
        if (subscription.isActive && (header.serviceBits & SERVICE_BIT_UNSUBSCRIBE))
        {
            if (header.packetId == subscription.subscribedPacketId)
            {
                subscription.pendingUnsubscribe = true;
                SendResponse(responsePacketType, subscription.subscribedPacketId, СombineHigh4Low4Bits(SERVICE_BIT_UNSUBSCRIBED, header.serviceBits));
            }
            return;
        }
        
        if (!subscription.isActive && (header.serviceBits & 0xF0) == EMPTY_SERVICE_BITS)
        {
            SendResponse(responsePacketType, header.packetId, СombineHigh4Low4Bits(EMPTY_SERVICE_BITS, header.serviceBits));
            return;
        }
    }

    void SubscriptionHandler::ProcessSubscription()
    {
        if (!subscription.isActive)
            return;
        
        unsigned long currentTime = millis();
        
        if (subscription.pendingUnsubscribe)
        {
            ResetSubscription();
            return;
        }
        
        if (currentTime - subscription.lastKeepAliveTime > keepAliveTimeout)
        {
            SendResponse(responsePacketType, subscription.subscribedPacketId, SERVICE_BIT_UNSUBSCRIBED);
            ResetSubscription();
            return;
        }
        
        if (!subscription.pendingUnsubscribe && currentTime - subscription.lastSendTime > sendInterval)
        {
            SendResponse(responsePacketType, subscription.subscribedPacketId, EMPTY_SERVICE_BITS);
            subscription.lastSendTime = currentTime;
        }
    }
}