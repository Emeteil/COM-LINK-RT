#include "distance.h"
#include "../core/subscription_base.h"
#include "../core/globals.h"
#include <Arduino.h>

namespace CommandDistance
{
    static constexpr uint32_t MEASURE_INTERVAL_US = 50000UL;
    static constexpr uint32_t ECHO_TIMEOUT_US     = 30000UL;
    static constexpr uint32_t TRIG_PULSE_US       = 10UL;

    enum class MeasureState : uint8_t
    {
        IDLE,
        TRIG_PENDING,
        WAITING_ECHO
    };

    static volatile uint32_t echoStartUs = 0;
    static volatile uint32_t echoDurationUs = 0;
    static volatile bool echoReady = false;

    static MeasureState measureState = MeasureState::IDLE;
    static uint32_t lastTrigUs = 0;
    static uint16_t cachedDistanceCm = 0;

    static void EchoISR()
    {
        if (digitalRead(ECHO_PIN) == HIGH)
        {
            echoStartUs = micros();
        }
        else
        {
            uint32_t now = micros();
            echoDurationUs = now - echoStartUs;
            echoReady = true;
        }
    }

    static void StartMeasurement()
    {
        echoReady = false;
        digitalWrite(TRIG_PIN, LOW);
        delayMicroseconds(2);
        digitalWrite(TRIG_PIN, HIGH);
        delayMicroseconds(TRIG_PULSE_US);
        digitalWrite(TRIG_PIN, LOW);
        lastTrigUs = micros();
        measureState = MeasureState::WAITING_ECHO;
    }

    static void TickMeasurement()
    {
        uint32_t now = micros();

        if (echoReady)
        {
            noInterrupts();
            uint32_t dur = echoDurationUs;
            echoReady = false;
            interrupts();

            uint32_t cm = (dur * 10UL) / 583UL;
            if (cm > 0xFFFF) cm = 0xFFFF;
            cachedDistanceCm = static_cast<uint16_t>(cm);
            measureState = MeasureState::IDLE;
        }
        else if (measureState == MeasureState::WAITING_ECHO && (now - lastTrigUs) > ECHO_TIMEOUT_US)
        {
            measureState = MeasureState::IDLE;
        }

        if (measureState == MeasureState::IDLE && (now - lastTrigUs) >= MEASURE_INTERVAL_US)
        {
            StartMeasurement();
        }
    }

    class DistanceHandler : public CommandSubscription::SubscriptionHandler
    {
        protected:
        void SendResponse(uint8_t packetType, uint16_t packetId, uint8_t serviceBits) override
        {
            uint8_t txBuffer[64];
            uint16_t length;

            uint16_t cm = cachedDistanceCm;

            protocol.parser.CreatePacket(
                PROTOCOL_VERSION,
                packetType,
                serviceBits,
                packetId,
                reinterpret_cast<const uint8_t*>(&cm),
                sizeof(cm),
                txBuffer,
                length
            );

            protocol.SendPacket(txBuffer, length, packetId);
        }

        public:
        DistanceHandler() : SubscriptionHandler(500, SUBSCRIPTION_KEEP_ALIVE_TIMEOUT_MS, PACKET_TYPE_RESPONSE) {}
    };

    DistanceHandler distanceHandler;

    void Init()
    {
        pinMode(TRIG_PIN, OUTPUT);
        pinMode(ECHO_PIN, INPUT);
        digitalWrite(TRIG_PIN, LOW);
        attachInterrupt(digitalPinToInterrupt(ECHO_PIN), EchoISR, CHANGE);
        lastTrigUs = micros();
    }

    void Handler(const ComLinkRTProtocol::PacketHeader& header, const uint8_t* data)
    {
        distanceHandler.HandleSubscription(header, data);
    }

    void Processor()
    {
        TickMeasurement();
        distanceHandler.ProcessSubscription();
    }
}
