#include "distance.h"
#include "../core/subscription_base.h"
#include "../core/globals.h"
#include <Arduino.h>

namespace CommandDistance
{
    static_assert(ULTRASONIC_COUNT % 2 == 0, "sensor count must be even for interleaved polling");
    static_assert(ULTRASONIC_COUNT <= 8, "only S0-S2 are wired");

    static constexpr uint32_t MUX_SETTLE_US = 5;
    static constexpr uint32_t TRIG_PULSE_US = 10;

    enum EdgeState : uint8_t
    {
        EDGE_IDLE,
        EDGE_WAIT_RISE,
        EDGE_WAIT_FALL,
        EDGE_DONE
    };

    enum class Phase : uint8_t
    {
        SELECT,
        SETTLE,
        TRIGGER,
        WAIT_ECHO,
        WAIT_SLOT_END
    };

    static volatile uint8_t edgeState = EDGE_IDLE;
    static volatile uint32_t riseTicks = 0;
    static volatile uint32_t fallTicks = 0;

    static Phase phase = Phase::SELECT;
    static uint8_t slot = 0;
    static uint32_t slotStartUs = 0;
    static uint32_t trigStartUs = 0;
    static uint32_t ticksPerSecond = 1000000UL;
    static uint16_t distances[ULTRASONIC_COUNT];

#ifdef UNIT_TEST
    static inline uint32_t Ticks() { return micros(); }
    static void StartTickCounter() { ticksPerSecond = 1000000UL; }
#else
    static inline uint32_t Ticks() { return DWT->CYCCNT; }
    static void StartTickCounter()
    {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
        ticksPerSecond = SystemCoreClock;
    }
#endif

    // Соседние по времени замеры идут с противоположных сторон робота: 0, 4, 1, 5, 2, 6, 3, 7
    static inline uint8_t SensorForSlot(uint8_t index)
    {
        return (index >> 1) + ((index & 1) ? ULTRASONIC_COUNT / 2 : 0);
    }

    static void EchoIsr()
    {
        const uint32_t now = Ticks();
        const bool high = digitalRead(ULTRASONIC_ECHO_PIN) == HIGH;

        if (edgeState == EDGE_WAIT_RISE && high)
        {
            riseTicks = now;
            edgeState = EDGE_WAIT_FALL;
        }
        else if (edgeState == EDGE_WAIT_FALL && !high)
        {
            fallTicks = now;
            edgeState = EDGE_DONE;
        }
    }

    static uint16_t ToMillimetres(uint32_t ticks)
    {
        const uint64_t scaled = static_cast<uint64_t>(ticks) * ULTRASONIC_SOUND_SPEED_MM_S;
        const uint64_t divisor = 2ULL * ticksPerSecond;
        uint64_t mm = (scaled + divisor / 2) / divisor;

        if (mm == DISTANCE_NO_ECHO)
            mm = 1;
        if (mm >= DISTANCE_ERROR)
            mm = DISTANCE_ERROR - 1;
        return static_cast<uint16_t>(mm);
    }

    static void SelectSensor(uint8_t sensor)
    {
        digitalWrite(ULTRASONIC_S0_PIN, (sensor & 0x01) ? HIGH : LOW);
        digitalWrite(ULTRASONIC_S1_PIN, (sensor & 0x02) ? HIGH : LOW);
        digitalWrite(ULTRASONIC_S2_PIN, (sensor & 0x04) ? HIGH : LOW);
    }

    static void FinishSlot(uint16_t value)
    {
        distances[SensorForSlot(slot)] = value;
        phase = Phase::WAIT_SLOT_END;
    }

    static void WaitEcho(uint32_t now)
    {
        const bool timedOut = now - trigStartUs >= ULTRASONIC_ECHO_TIMEOUT_US;

        noInterrupts();
        const uint8_t state = edgeState;
        const uint32_t width = fallTicks - riseTicks;
        if (state != EDGE_DONE && timedOut)
            edgeState = EDGE_IDLE;
        interrupts();

        if (state == EDGE_DONE)
            FinishSlot(ToMillimetres(width));
        else if (timedOut)
            FinishSlot(state == EDGE_WAIT_FALL ? DISTANCE_NO_ECHO : DISTANCE_ERROR);
    }

    static void TickMeasurement()
    {
        const uint32_t now = micros();

        switch (phase)
        {
            case Phase::SELECT:
                edgeState = EDGE_IDLE;
                SelectSensor(SensorForSlot(slot));
                slotStartUs = now;
                phase = Phase::SETTLE;
                break;

            case Phase::SETTLE:
                if (now - slotStartUs < MUX_SETTLE_US)
                    break;
                // эхо выбранного датчика уже поднято: замер не начнётся, пропускаем слот
                if (digitalRead(ULTRASONIC_ECHO_PIN) == HIGH)
                {
                    FinishSlot(DISTANCE_ERROR);
                    break;
                }
                edgeState = EDGE_WAIT_RISE;
                digitalWrite(ULTRASONIC_TRIG_PIN, HIGH);
                trigStartUs = now;
                phase = Phase::TRIGGER;
                break;

            case Phase::TRIGGER:
                if (now - trigStartUs < TRIG_PULSE_US)
                    break;
                digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
                phase = Phase::WAIT_ECHO;
                break;

            case Phase::WAIT_ECHO:
                WaitEcho(now);
                break;

            case Phase::WAIT_SLOT_END:
                if (now - slotStartUs < ULTRASONIC_SLOT_US)
                    break;
                slot = (slot + 1) % ULTRASONIC_COUNT;
                phase = Phase::SELECT;
                break;
        }
    }

    class DistanceHandler : public CommandSubscription::SubscriptionHandler
    {
        protected:
        void SendResponse(uint8_t packetType, uint16_t packetId, uint8_t serviceBits) override
        {
            uint8_t txBuffer[64];
            uint16_t length;

            protocol.parser.CreatePacket(
                PROTOCOL_VERSION,
                packetType,
                serviceBits,
                packetId,
                reinterpret_cast<const uint8_t*>(distances),
                sizeof(distances),
                txBuffer,
                length);

            protocol.SendPacket(txBuffer, length, packetId);
        }

        public:
        DistanceHandler() : SubscriptionHandler(ULTRASONIC_SEND_INTERVAL_MS, SUBSCRIPTION_KEEP_ALIVE_TIMEOUT_MS, PACKET_TYPE_RESPONSE) {}
    };

    DistanceHandler distanceHandler;

    void Reset()
    {
        distanceHandler.ResetSubscription();
        edgeState = EDGE_IDLE;
        phase = Phase::SELECT;
        slot = 0;
        for (uint8_t i = 0; i < ULTRASONIC_COUNT; i++)
            distances[i] = DISTANCE_ERROR;
    }

    void Init()
    {
        Reset();
        StartTickCounter();

        pinMode(ULTRASONIC_S0_PIN, OUTPUT);
        pinMode(ULTRASONIC_S1_PIN, OUTPUT);
        pinMode(ULTRASONIC_S2_PIN, OUTPUT);
        pinMode(ULTRASONIC_TRIG_PIN, OUTPUT);
        digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
        pinMode(ULTRASONIC_ECHO_PIN, INPUT);

        attachInterrupt(digitalPinToInterrupt(ULTRASONIC_ECHO_PIN), EchoIsr, CHANGE);
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
