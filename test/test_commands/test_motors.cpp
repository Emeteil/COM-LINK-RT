#include <gtest/gtest.h>
#include "commands_fixture.h"

using TestSupport::CommandsFixture;
using TestSupport::ExpectPacket;
using TestSupport::PacketBuilder;

namespace
{
    const uint8_t CHANNEL_MOTOR1 = 0;
    const uint8_t CHANNEL_MOTOR2 = 1;

    const uint16_t PULSE_NEUTRAL = (PWM_SERVO_MIN + PWM_SERVO_MAX) / 2;
    const uint16_t PULSE_HALF_RANGE = (PWM_SERVO_MAX - PWM_SERVO_MIN) / 2;

    uint16_t ExpectedPulse(uint8_t direction, uint8_t speed)
    {
        uint16_t offset = static_cast<uint16_t>(static_cast<uint32_t>(speed) * PULSE_HALF_RANGE / 255);

        if (direction == CommandMotors::DIRECTION_FORWARD)
            return PULSE_NEUTRAL + offset;
        if (direction == CommandMotors::DIRECTION_BACKWARD)
            return PULSE_NEUTRAL - offset;
        return PULSE_NEUTRAL;
    }

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
    };
}

TEST_F(MotorsTest, InitialisesPwmDriverAndStopsMotors)
{
    ClearPwmLog();
    InitMotors();

    EXPECT_TRUE(MockPwm::Get().begun);
    EXPECT_EQ(MockPwm::Get().address, PWM_SERVO_ADDRESS);
    EXPECT_FLOAT_EQ(MockPwm::Get().frequencyHz, PWM_SERVO_FREQ_HZ);
    EXPECT_EQ(MockPwm::ChannelOff(CHANNEL_MOTOR1), PULSE_NEUTRAL);
    EXPECT_EQ(MockPwm::ChannelOff(CHANNEL_MOTOR2), PULSE_NEUTRAL);
}

TEST_F(MotorsTest, AnswersWithResponsePacketType)
{
    SendCommand(MakeCommand(CommandMotors::COMMAND_STOP_ALL, 0x03, 0, 0, 0, 0), 0x0077);

    ASSERT_EQ(SentCount(), 1u);
    ExpectPacket(LastSent(), CommandMotors::PACKET_TYPE_RESPONSE, 0x0077);
    EXPECT_EQ(LastSent().header.dataLength, 0u);
}

TEST_F(MotorsTest, SetBothDrivesBothChannels)
{
    SendCommand(MakeCommand(CommandMotors::COMMAND_SET_BOTH, 0x03,
                            CommandMotors::DIRECTION_FORWARD, CommandMotors::DIRECTION_BACKWARD,
                            255, 128));

    EXPECT_EQ(MockPwm::ChannelOff(CHANNEL_MOTOR1), ExpectedPulse(CommandMotors::DIRECTION_FORWARD, 255));
    EXPECT_EQ(MockPwm::ChannelOff(CHANNEL_MOTOR2), ExpectedPulse(CommandMotors::DIRECTION_BACKWARD, 128));
}

TEST_F(MotorsTest, FullForwardReachesMaximumPulse)
{
    SendCommand(MakeCommand(CommandMotors::COMMAND_SET_BOTH, MOTOR1_MASK,
                            CommandMotors::DIRECTION_FORWARD, 0, 255, 0));

    EXPECT_EQ(MockPwm::ChannelOff(CHANNEL_MOTOR1), PWM_SERVO_MAX);
}

TEST_F(MotorsTest, MaskLimitsCommandToSelectedMotor)
{
    SendCommand(MakeCommand(CommandMotors::COMMAND_SET_BOTH, MOTOR1_MASK,
                            CommandMotors::DIRECTION_FORWARD, CommandMotors::DIRECTION_FORWARD,
                            200, 200));

    EXPECT_EQ(MockPwm::ChannelOff(CHANNEL_MOTOR1), ExpectedPulse(CommandMotors::DIRECTION_FORWARD, 200));
    EXPECT_FALSE(MockPwm::HasChannel(CHANNEL_MOTOR2));
}

TEST_F(MotorsTest, DifferentialIgnoresMaskAndDrivesBoth)
{
    SendCommand(MakeCommand(CommandMotors::COMMAND_SET_DIFFERENTIAL, 0x00,
                            CommandMotors::DIRECTION_FORWARD, CommandMotors::DIRECTION_BACKWARD,
                            100, 50));

    EXPECT_EQ(MockPwm::ChannelOff(CHANNEL_MOTOR1), ExpectedPulse(CommandMotors::DIRECTION_FORWARD, 100));
    EXPECT_EQ(MockPwm::ChannelOff(CHANNEL_MOTOR2), ExpectedPulse(CommandMotors::DIRECTION_BACKWARD, 50));
}

TEST_F(MotorsTest, SetSpeedKeepsPreviousDirection)
{
    SendCommand(MakeCommand(CommandMotors::COMMAND_SET_DIRECTION, MOTOR1_MASK,
                            CommandMotors::DIRECTION_BACKWARD, 0, 0, 0));
    SendCommand(MakeCommand(CommandMotors::COMMAND_SET_SPEED, MOTOR1_MASK, 0, 0, 255, 0));

    EXPECT_EQ(MockPwm::ChannelOff(CHANNEL_MOTOR1), ExpectedPulse(CommandMotors::DIRECTION_BACKWARD, 255));
}

TEST_F(MotorsTest, StopAllReturnsBothChannelsToNeutral)
{
    SendCommand(MakeCommand(CommandMotors::COMMAND_SET_BOTH, 0x03,
                            CommandMotors::DIRECTION_FORWARD, CommandMotors::DIRECTION_FORWARD,
                            255, 255));

    SendCommand(MakeCommand(CommandMotors::COMMAND_STOP_ALL, 0x00, 0, 0, 0, 0));

    EXPECT_EQ(MockPwm::ChannelOff(CHANNEL_MOTOR1), PULSE_NEUTRAL);
    EXPECT_EQ(MockPwm::ChannelOff(CHANNEL_MOTOR2), PULSE_NEUTRAL);
}

TEST_F(MotorsTest, BrakeAndStopUseNeutralPulse)
{
    SendCommand(MakeCommand(CommandMotors::COMMAND_SET_BOTH, 0x03,
                            CommandMotors::DIRECTION_BRAKE, CommandMotors::DIRECTION_STOP,
                            255, 255));

    EXPECT_EQ(MockPwm::ChannelOff(CHANNEL_MOTOR1), PULSE_NEUTRAL);
    EXPECT_EQ(MockPwm::ChannelOff(CHANNEL_MOTOR2), PULSE_NEUTRAL);
}

TEST_F(MotorsTest, InvalidDirectionIsIgnored)
{
    SendCommand(MakeCommand(CommandMotors::COMMAND_SET_DIRECTION, MOTOR1_MASK, 0x7F, 0, 0, 0));

    EXPECT_FALSE(MockPwm::HasChannel(CHANNEL_MOTOR1));
}

TEST_F(MotorsTest, RejectsPayloadOfWrongSize)
{
    Exchange(PacketBuilder(CommandMotors::PACKET_TYPE, 0x0001).Payload({0x01, 0x02}));

    EXPECT_EQ(SentCount(), 0u);
    EXPECT_EQ(MockPwm::CallCount(), 0u);
}

TEST_F(MotorsTest, RejectsEmptyPayload)
{
    Exchange(PacketBuilder(CommandMotors::PACKET_TYPE, 0x0001));

    EXPECT_EQ(SentCount(), 0u);
    EXPECT_EQ(MockPwm::CallCount(), 0u);
}

TEST_F(MotorsTest, AppliesCommandButStaysSilentForZeroPacketId)
{
    SendCommand(MakeCommand(CommandMotors::COMMAND_SET_BOTH, MOTOR1_MASK,
                            CommandMotors::DIRECTION_FORWARD, 0, 255, 0),
                ZERO_PACKET_ID);

    EXPECT_EQ(SentCount(), 0u);
    EXPECT_EQ(MockPwm::ChannelOff(CHANNEL_MOTOR1), ExpectedPulse(CommandMotors::DIRECTION_FORWARD, 255));
}

TEST_F(MotorsTest, DoesNothingWhenPwmDriverIsMissing)
{
    MockPwm::Get().beginResult = false;
    InitMotors();
    ClearPwmLog();

    SendCommand(MakeCommand(CommandMotors::COMMAND_SET_BOTH, 0x03,
                            CommandMotors::DIRECTION_FORWARD, CommandMotors::DIRECTION_FORWARD,
                            255, 255));

    EXPECT_EQ(SentCount(), 0u);
    EXPECT_EQ(MockPwm::CallCount(), 0u);
}

TEST_F(MotorsTest, RecoversAfterPwmDriverBecomesAvailable)
{
    MockPwm::Get().beginResult = false;
    InitMotors();

    MockPwm::Get().beginResult = true;
    InitMotors();
    ClearPwmLog();

    SendCommand(MakeCommand(CommandMotors::COMMAND_SET_BOTH, MOTOR1_MASK,
                            CommandMotors::DIRECTION_FORWARD, 0, 255, 0));

    EXPECT_EQ(SentCount(), 1u);
    EXPECT_EQ(MockPwm::ChannelOff(CHANNEL_MOTOR1), ExpectedPulse(CommandMotors::DIRECTION_FORWARD, 255));
}
