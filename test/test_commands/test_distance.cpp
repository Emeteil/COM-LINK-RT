#include <gtest/gtest.h>
#include <vector>
#include "commands_fixture.h"

using TestSupport::CommandsFixture;
using TestSupport::ExpectPacket;
using TestSupport::PacketBuilder;

namespace
{
    const long SILENT = -1;  // датчик не поднимает эхо
    const long ENDLESS = -2; // эхо поднимается и не опускается до таймаута

    const unsigned long ECHO_DELAY_US = 450;
    const unsigned long SWEEP_US = ULTRASONIC_SLOT_US * ULTRASONIC_COUNT;

    uint16_t ExpectedMm(unsigned long widthUs)
    {
        return static_cast<uint16_t>((static_cast<uint64_t>(widthUs) * ULTRASONIC_SOUND_SPEED_MM_S + 1000000ULL) / 2000000ULL);
    }

    struct Trigger
    {
        uint8_t sensor;
        unsigned long riseUs;
        unsigned long fallUs;
    };

    class DistanceTest : public CommandsFixture
    {
        protected:
        long widths[ULTRASONIC_COUNT];
        std::vector<Trigger> triggers;
        int lastTrig = LOW;
        unsigned long echoRiseAt = 0;
        unsigned long echoFallAt = 0;
        bool echoPending = false;
        bool echoHigh = false;

        void SetUp() override
        {
            CommandsFixture::SetUp();
            for (long& w : widths) w = 0;
            triggers.clear();
            lastTrig = LOW;
            echoPending = echoHigh = false;
        }

        uint8_t Address() const
        {
            return static_cast<uint8_t>(MockArduino::PinValue(ULTRASONIC_S0_PIN) |
                                        (MockArduino::PinValue(ULTRASONIC_S1_PIN) << 1) |
                                        (MockArduino::PinValue(ULTRASONIC_S2_PIN) << 2));
        }

        void SimulateSensors()
        {
            const unsigned long now = MockArduino::Micros();
            const int trig = MockArduino::PinValue(ULTRASONIC_TRIG_PIN);

            if (lastTrig == LOW && trig == HIGH)
                triggers.push_back({Address(), now, 0});

            if (lastTrig == HIGH && trig == LOW)
            {
                triggers.back().fallUs = now;
                const long width = widths[Address()];
                echoPending = width != SILENT;
                echoRiseAt = now + ECHO_DELAY_US;
                echoFallAt = (width == ENDLESS) ? now + 10 * ULTRASONIC_SLOT_US : echoRiseAt + width;
            }
            lastTrig = trig;

            if (echoPending && !echoHigh && now >= echoRiseAt)
            {
                echoHigh = true;
                MockArduino::DriveInterruptPin(ULTRASONIC_ECHO_PIN, HIGH);
            }
            if (echoPending && echoHigh && now >= echoFallAt)
            {
                echoHigh = echoPending = false;
                MockArduino::DriveInterruptPin(ULTRASONIC_ECHO_PIN, LOW);
            }
        }

        void Run(unsigned long us)
        {
            for (unsigned long t = 0; t < us; t++)
            {
                MockArduino::AdvanceMicros(1);
                SimulateSensors();
                protocol.Update();
            }
        }

        std::vector<uint16_t> Request()
        {
            Exchange(PacketBuilder(CommandDistance::PACKET_TYPE, 0x0042));
            const TestSupport::SentPacket& packet = LastSent();
            std::vector<uint16_t> values(packet.payload.size() / 2);
            memcpy(values.data(), packet.payload.data(), values.size() * 2);
            return values;
        }
    };
}

TEST_F(DistanceTest, InitConfiguresMuxPinsAndEchoInterrupt)
{
    MockArduino::ResetAll();
    CommandDistance::Init();

    for (uint16_t pin : {ULTRASONIC_S0_PIN, ULTRASONIC_S1_PIN, ULTRASONIC_S2_PIN, ULTRASONIC_TRIG_PIN})
    {
        bool output = false;
        for (const MockArduino::PinEvent& event : MockArduino::Get().pinModes)
            output |= event.pin == pin && event.value == OUTPUT;
        EXPECT_TRUE(output) << pin;
    }
    EXPECT_EQ(MockArduino::LastDigitalWrite(ULTRASONIC_TRIG_PIN), LOW);
    EXPECT_EQ(MockArduino::Get().interruptModes[ULTRASONIC_ECHO_PIN], CHANGE);
}

TEST_F(DistanceTest, ReportsErrorBeforeFirstMeasurement)
{
    std::vector<uint16_t> values = Request();

    ASSERT_EQ(values.size(), static_cast<size_t>(ULTRASONIC_COUNT));
    ExpectPacket(LastSent(), CommandDistance::PACKET_TYPE_RESPONSE, 0x0042);
    for (uint16_t v : values)
        EXPECT_EQ(v, CommandDistance::DISTANCE_ERROR);
}

TEST_F(DistanceTest, MeasuresEverySensorInMillimetres)
{
    const long sample[ULTRASONIC_COUNT] = {583, 1166, 2915, 5831, 11662, 17493, 20000, 23000};
    for (uint8_t i = 0; i < ULTRASONIC_COUNT; i++) widths[i] = sample[i];

    Run(SWEEP_US + 100);

    std::vector<uint16_t> values = Request();
    for (uint8_t i = 0; i < ULTRASONIC_COUNT; i++)
        EXPECT_EQ(values[i], ExpectedMm(sample[i])) << "sensor " << int(i);
    EXPECT_EQ(values[3], 1001);
}

TEST_F(DistanceTest, PollsSensorsInterleavedAcrossTheRobot)
{
    Run(SWEEP_US);

    ASSERT_EQ(triggers.size(), static_cast<size_t>(ULTRASONIC_COUNT));
    const uint8_t expected[ULTRASONIC_COUNT] = {0, 4, 1, 5, 2, 6, 3, 7};
    for (uint8_t i = 0; i < ULTRASONIC_COUNT; i++)
        EXPECT_EQ(triggers[i].sensor, expected[i]);
}

TEST_F(DistanceTest, KeepsFixedSlotPerSensor)
{
    for (long& w : widths) w = 23000;

    Run(SWEEP_US * 2);

    ASSERT_GE(triggers.size(), static_cast<size_t>(ULTRASONIC_COUNT + 1));
    for (size_t i = 1; i < triggers.size(); i++)
    {
        const unsigned long gap = triggers[i].riseUs - triggers[i - 1].riseUs;
        EXPECT_GE(gap, ULTRASONIC_SLOT_US);
        EXPECT_LE(gap, ULTRASONIC_SLOT_US + 10);
    }
}

TEST_F(DistanceTest, TriggerPulseIsAtLeastTenMicroseconds)
{
    Run(SWEEP_US);

    for (const Trigger& t : triggers)
    {
        EXPECT_GE(t.fallUs - t.riseUs, 10u);
        EXPECT_LE(t.fallUs - t.riseUs, 20u);
    }
}

TEST_F(DistanceTest, EchoThatNeverEndsMeansNothingInRange)
{
    widths[4] = ENDLESS;

    Run(SWEEP_US + 100);

    EXPECT_EQ(Request()[4], CommandDistance::DISTANCE_NO_ECHO);
}

TEST_F(DistanceTest, SilentSensorIsReportedAsError)
{
    widths[2] = SILENT;
    widths[3] = 5831;

    Run(SWEEP_US + 100);

    std::vector<uint16_t> values = Request();
    EXPECT_EQ(values[2], CommandDistance::DISTANCE_ERROR);
    EXPECT_EQ(values[3], ExpectedMm(5831));
}

TEST_F(DistanceTest, EchoAlreadyHighSkipsTriggerAndReportsError)
{
    MockArduino::SetPinValue(ULTRASONIC_ECHO_PIN, HIGH);

    Run(ULTRASONIC_SLOT_US);

    EXPECT_TRUE(triggers.empty());
    MockArduino::SetPinValue(ULTRASONIC_ECHO_PIN, LOW);
    EXPECT_EQ(Request()[0], CommandDistance::DISTANCE_ERROR);
}

TEST_F(DistanceTest, EdgesOutsideMeasurementAreIgnored)
{
    widths[0] = 5831;
    Run(ULTRASONIC_SLOT_US);

    MockArduino::DriveInterruptPin(ULTRASONIC_ECHO_PIN, HIGH);
    MockArduino::AdvanceMicros(100);
    MockArduino::DriveInterruptPin(ULTRASONIC_ECHO_PIN, LOW);
    Run(10);

    EXPECT_EQ(Request()[0], ExpectedMm(5831));
}

TEST_F(DistanceTest, FreshValueReplacesOldOnNextSweep)
{
    widths[1] = 5831;
    Run(SWEEP_US + 100);
    widths[1] = 2915;
    Run(SWEEP_US);

    EXPECT_EQ(Request()[1], ExpectedMm(2915));
}

TEST_F(DistanceTest, SubscriptionStreamsAllSensorsAboutFourTimesPerSecond)
{
    for (long& w : widths) w = 5831;

    Exchange(PacketBuilder(CommandDistance::PACKET_TYPE, 0x0033).ServiceBits(SERVICE_BIT_SUBSCRIBE));
    ASSERT_EQ(SentCount(), 1u);

    Run(1000000);

    const size_t streamed = SentCount() - 1;
    EXPECT_GE(streamed, 3u);
    EXPECT_LE(streamed, 4u);
    ExpectPacket(LastSent(), CommandDistance::PACKET_TYPE_RESPONSE, 0x0033);
    EXPECT_EQ(LastSent().payload.size(), ULTRASONIC_COUNT * sizeof(uint16_t));
    EXPECT_EQ(LastSent().PayloadAs<uint16_t>(), ExpectedMm(5831));
}

TEST_F(DistanceTest, UnsubscribeStopsStream)
{
    Exchange(PacketBuilder(CommandDistance::PACKET_TYPE, 0x0033).ServiceBits(SERVICE_BIT_SUBSCRIBE));
    Exchange(PacketBuilder(CommandDistance::PACKET_TYPE, 0x0033).ServiceBits(SERVICE_BIT_UNSUBSCRIBE));
    EXPECT_EQ(LastSent().header.serviceBits, SERVICE_BIT_UNSUBSCRIBED);

    const size_t after = SentCount();
    Run(600000);
    EXPECT_EQ(SentCount(), after);
}
