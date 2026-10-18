#include <gtest/gtest.h>
#include "commands_fixture.h"

using MockArduino::DigitalWriteCount;
using MockArduino::LastAnalogWrite;
using MockArduino::LastDigitalWrite;
using TestSupport::CommandsFixture;
using TestSupport::ExpectPacket;
using TestSupport::PacketBuilder;

namespace
{
    struct Pins
    {
        uint16_t ena;
        uint16_t in1;
        uint16_t in2;
    };

    const Pins MOTOR1 = {MOTOR1_ENA_PIN, MOTOR1_IN1_PIN, MOTOR1_IN2_PIN};
    const Pins MOTOR2 = {MOTOR2_ENA_PIN, MOTOR2_IN1_PIN, MOTOR2_IN2_PIN};

    CommandMotors::MotorCommand MakeCommand(uint8_t commandType, uint8_t motorMask,
                                            uint8_t direction1, uint8_t direction2,
                                            uint8_t speed1, uint8_t speed2)
    {
        CommandMotors::MotorCommand command;
        command.commandType = static_cast<CommandMotors::MotorCommandType>(commandType);
        command.motorMask = motorMask;
        command.direction1 = direction1;
        command.direction2 = direction2;
        command.speed1 = speed1;
        command.speed2 = speed2;
        return command;
    }

    class MotorsTest : public CommandsFixture
    {
        protected:
        void SendCommand(const CommandMotors::MotorCommand& command, uint16_t packetId = 0x0001)
        {
            Exchange(PacketBuilder(CommandMotors::PACKET_TYPE, packetId).PayloadOf(command));
        }

        void ExpectDirection(const Pins& pins, int in1, int in2)
        {
            EXPECT_EQ(LastDigitalWrite(pins.in1), in1);
            EXPECT_EQ(LastDigitalWrite(pins.in2), in2);
        }

        size_t MotorWrites(const Pins& pins)
        {
            return DigitalWriteCount(pins.in1) + DigitalWriteCount(pins.in2) +
                   (LastAnalogWrite(pins.ena) == -1 ? 0 : 1);
        }
    };
}

TEST_F(MotorsTest, InitConfiguresBridgePinsAndStopsMotors)
{
    ClearPinLog();
    InitMotors();

    for (const Pins& pins : {MOTOR1, MOTOR2})
    {
        int outputs = 0;
        for (const MockArduino::PinEvent& event : MockArduino::Get().pinModes)
            if ((event.pin == pins.ena || event.pin == pins.in1 || event.pin == pins.in2) && event.value == OUTPUT)
                outputs++;
        EXPECT_EQ(outputs, 3);
        ExpectDirection(pins, LOW, LOW);
        EXPECT_EQ(LastAnalogWrite(pins.ena), 0);
    }
}

TEST_F(MotorsTest, AnswersWithResponsePacketType)
{
    SendCommand(MakeCommand(CommandMotors::COMMAND_STOP_ALL, 0x03, 0, 0, 0, 0), 0x0077);

    ASSERT_EQ(SentCount(), 1u);
    ExpectPacket(LastSent(), CommandMotors::PACKET_TYPE_RESPONSE, 0x0077);
    EXPECT_EQ(LastSent().header.dataLength, 0u);
}

TEST_F(MotorsTest, SetBothDrivesBothBridges)
{
    SendCommand(MakeCommand(CommandMotors::COMMAND_SET_BOTH, 0x03,
                            CommandMotors::DIRECTION_FORWARD, CommandMotors::DIRECTION_BACKWARD,
                            255, 128));

    ExpectDirection(MOTOR1, HIGH, LOW);
    ExpectDirection(MOTOR2, LOW, HIGH);
    EXPECT_EQ(LastAnalogWrite(MOTOR1.ena), 255);
    EXPECT_EQ(LastAnalogWrite(MOTOR2.ena), 128);
}

TEST_F(MotorsTest, BrakeShortsBothInputsHigh)
{
    SendCommand(MakeCommand(CommandMotors::COMMAND_SET_DIRECTION, 0x03,
                            CommandMotors::DIRECTION_BRAKE, CommandMotors::DIRECTION_STOP, 0, 0));

    ExpectDirection(MOTOR1, HIGH, HIGH);
    ExpectDirection(MOTOR2, LOW, LOW);
}

TEST_F(MotorsTest, MaskLimitsCommandToSelectedMotor)
{
    SendCommand(MakeCommand(CommandMotors::COMMAND_SET_BOTH, MOTOR1_MASK,
                            CommandMotors::DIRECTION_FORWARD, CommandMotors::DIRECTION_FORWARD,
                            200, 200));

    ExpectDirection(MOTOR1, HIGH, LOW);
    EXPECT_EQ(LastAnalogWrite(MOTOR1.ena), 200);
    EXPECT_EQ(MotorWrites(MOTOR2), 0u);
}

TEST_F(MotorsTest, DifferentialIgnoresMaskAndDrivesBoth)
{
    SendCommand(MakeCommand(CommandMotors::COMMAND_SET_DIFFERENTIAL, 0x00,
                            CommandMotors::DIRECTION_FORWARD, CommandMotors::DIRECTION_BACKWARD,
                            100, 50));

    ExpectDirection(MOTOR1, HIGH, LOW);
    ExpectDirection(MOTOR2, LOW, HIGH);
    EXPECT_EQ(LastAnalogWrite(MOTOR1.ena), 100);
    EXPECT_EQ(LastAnalogWrite(MOTOR2.ena), 50);
}

TEST_F(MotorsTest, SetSpeedKeepsPreviousDirection)
{
    SendCommand(MakeCommand(CommandMotors::COMMAND_SET_DIRECTION, MOTOR1_MASK,
                            CommandMotors::DIRECTION_BACKWARD, 0, 0, 0));
    SendCommand(MakeCommand(CommandMotors::COMMAND_SET_SPEED, MOTOR1_MASK, 0, 0, 255, 0));

    ExpectDirection(MOTOR1, LOW, HIGH);
    EXPECT_EQ(LastAnalogWrite(MOTOR1.ena), 255);
}

TEST_F(MotorsTest, StopAllReleasesBothBridges)
{
    SendCommand(MakeCommand(CommandMotors::COMMAND_SET_BOTH, 0x03,
                            CommandMotors::DIRECTION_FORWARD, CommandMotors::DIRECTION_FORWARD,
                            255, 255));

    SendCommand(MakeCommand(CommandMotors::COMMAND_STOP_ALL, 0x00, 0, 0, 0, 0));

    for (const Pins& pins : {MOTOR1, MOTOR2})
    {
        ExpectDirection(pins, LOW, LOW);
        EXPECT_EQ(LastAnalogWrite(pins.ena), 0);
    }
}

TEST_F(MotorsTest, InvalidDirectionIsIgnored)
{
    SendCommand(MakeCommand(CommandMotors::COMMAND_SET_DIRECTION, MOTOR1_MASK, 0x7F, 0, 0, 0));

    EXPECT_EQ(MotorWrites(MOTOR1), 0u);
}

TEST_F(MotorsTest, RejectsPayloadOfWrongSize)
{
    Exchange(PacketBuilder(CommandMotors::PACKET_TYPE, 0x0001).Payload({0x01, 0x02}));

    EXPECT_EQ(SentCount(), 0u);
    EXPECT_EQ(MotorWrites(MOTOR1) + MotorWrites(MOTOR2), 0u);
}

TEST_F(MotorsTest, RejectsEmptyPayload)
{
    Exchange(PacketBuilder(CommandMotors::PACKET_TYPE, 0x0001));

    EXPECT_EQ(SentCount(), 0u);
    EXPECT_EQ(MotorWrites(MOTOR1) + MotorWrites(MOTOR2), 0u);
}

TEST_F(MotorsTest, AppliesCommandButStaysSilentForZeroPacketId)
{
    SendCommand(MakeCommand(CommandMotors::COMMAND_SET_BOTH, MOTOR1_MASK,
                            CommandMotors::DIRECTION_FORWARD, 0, 255, 0),
                ZERO_PACKET_ID);

    EXPECT_EQ(SentCount(), 0u);
    ExpectDirection(MOTOR1, HIGH, LOW);
    EXPECT_EQ(LastAnalogWrite(MOTOR1.ena), 255);
}
