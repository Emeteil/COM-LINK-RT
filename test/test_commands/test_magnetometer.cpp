#include <gtest/gtest.h>
#include <Wire.h>
#include "commands_fixture.h"

using CommandMagnetometer::MagnetometerData;
using TestSupport::CommandsFixture;
using TestSupport::ExpectPacket;
using TestSupport::PacketBuilder;

namespace
{
    const uint8_t ADDR = MAGNETOMETER_I2C_ADDRESS;

    class MagnetometerTest : public CommandsFixture
    {
        protected:
        void ReleaseBus()
        {
            MockArduino::SetPinValue(I2C1_SCL, HIGH);
            MockArduino::SetPinValue(I2C1_SDA, HIGH);
        }

        void PlugSensor()
        {
            ReleaseBus();
            MockWire::AddDevice(ADDR);
            MockWire::SetRegister(ADDR, 0x00, 0x80);
        }

        void SetSample(int16_t x, int16_t y, int16_t z, uint8_t status)
        {
            const int16_t values[3] = {x, y, z};
            for (uint8_t i = 0; i < 3; i++)
            {
                MockWire::SetRegister(ADDR, static_cast<uint8_t>(0x01 + i * 2), static_cast<uint8_t>(values[i] & 0xFF));
                MockWire::SetRegister(ADDR, static_cast<uint8_t>(0x02 + i * 2), static_cast<uint8_t>((values[i] >> 8) & 0xFF));
            }
            MockWire::SetRegister(ADDR, 0x09, status);
        }

        void Run(unsigned long ms)
        {
            for (unsigned long t = 0; t < ms; t++)
                Tick(1);
        }

        MagnetometerData Request()
        {
            Exchange(PacketBuilder(CommandMagnetometer::PACKET_TYPE, 0x0021));
            return LastSent().PayloadAs<MagnetometerData>();
        }
    };
}

TEST_F(MagnetometerTest, ConfiguresSensorOnStart)
{
    PlugSensor();
    CommandMagnetometer::Init();

    EXPECT_TRUE(MockWire::Get().begun);
    EXPECT_EQ(MockWire::Get().clockHz, MAGNETOMETER_I2C_CLOCK_HZ);
    EXPECT_EQ(MockWire::Register(ADDR, 0x29), 0x06);
    EXPECT_EQ(MockWire::Register(ADDR, 0x0B), 0x08);
    EXPECT_EQ(MockWire::Register(ADDR, 0x0A), 0xC5);
    EXPECT_EQ(Request().status, CommandMagnetometer::STATUS_NO_DATA);
}

TEST_F(MagnetometerTest, ReadsFieldWhenDataReady)
{
    PlugSensor();
    CommandMagnetometer::Init();
    SetSample(1000, -2000, 300, 0x01);

    Run(25);

    MagnetometerData data = Request();
    ExpectPacket(LastSent(), CommandMagnetometer::PACKET_TYPE_RESPONSE, 0x0021);
    EXPECT_EQ(LastSent().payload.size(), sizeof(MagnetometerData));
    EXPECT_EQ(data.x, 1000);
    EXPECT_EQ(data.y, -2000);
    EXPECT_EQ(data.z, 300);
    EXPECT_EQ(data.status, CommandMagnetometer::STATUS_OK);
}

TEST_F(MagnetometerTest, WaitsForDataReadyBit)
{
    PlugSensor();
    CommandMagnetometer::Init();
    SetSample(1000, 1000, 1000, 0x00);

    Run(100);

    EXPECT_EQ(Request().status, CommandMagnetometer::STATUS_NO_DATA);
}

TEST_F(MagnetometerTest, ReportsOverflow)
{
    PlugSensor();
    CommandMagnetometer::Init();
    SetSample(32767, 0, 0, 0x03);

    Run(25);

    EXPECT_EQ(Request().status, CommandMagnetometer::STATUS_OVERFLOW);
}

TEST_F(MagnetometerTest, StuckBusIsReportedWithoutTouchingI2c)
{
    MockArduino::SetPinValue(I2C1_SCL, LOW);
    MockArduino::SetPinValue(I2C1_SDA, HIGH);
    CommandMagnetometer::Init();
    Run(1500);

    EXPECT_EQ(Request().status, CommandMagnetometer::STATUS_BUS_STUCK);
    EXPECT_FALSE(MockWire::Get().begun);
    EXPECT_TRUE(MockWire::Get().transmissions.empty());
}

TEST_F(MagnetometerTest, MissingSensorIsFoundOnLaterProbe)
{
    ReleaseBus();
    CommandMagnetometer::Init();
    EXPECT_EQ(Request().status, CommandMagnetometer::STATUS_NOT_FOUND);

    PlugSensor();
    SetSample(5, 6, 7, 0x01);
    Run(MAGNETOMETER_PROBE_INTERVAL_MS + 30);

    MagnetometerData data = Request();
    EXPECT_EQ(data.status, CommandMagnetometer::STATUS_OK);
    EXPECT_EQ(data.z, 7);
}

TEST_F(MagnetometerTest, WrongChipIdIsNotAccepted)
{
    PlugSensor();
    MockWire::SetRegister(ADDR, 0x00, 0x12);
    CommandMagnetometer::Init();

    EXPECT_EQ(Request().status, CommandMagnetometer::STATUS_NOT_FOUND);
}

TEST_F(MagnetometerTest, UnpluggedSensorIsReportedAfterRepeatedErrors)
{
    PlugSensor();
    CommandMagnetometer::Init();
    SetSample(1, 2, 3, 0x01);
    Run(25);

    MockWire::RemoveDevice(ADDR);
    Run(100);

    EXPECT_EQ(Request().status, CommandMagnetometer::STATUS_NOT_FOUND);
}

TEST_F(MagnetometerTest, SubscriptionStreamsAboutTwentyTimesPerSecond)
{
    PlugSensor();
    CommandMagnetometer::Init();
    SetSample(10, 20, 30, 0x01);

    Exchange(PacketBuilder(CommandMagnetometer::PACKET_TYPE, 0x0055).ServiceBits(SERVICE_BIT_SUBSCRIBE));
    const size_t before = SentCount();
    Run(1000);

    const size_t streamed = SentCount() - before;
    EXPECT_GE(streamed, 18u);
    EXPECT_LE(streamed, 20u);
    ExpectPacket(LastSent(), CommandMagnetometer::PACKET_TYPE_RESPONSE, 0x0055);
    EXPECT_EQ(LastSent().PayloadAs<MagnetometerData>().y, 20);
}
