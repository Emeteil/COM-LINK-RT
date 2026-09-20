#pragma once
#include "../../src/commands/millis.h"
#include "../../src/commands/motors.h"
#include "../../src/commands/ping.h"
#include "../../src/commands/version.h"
#include "../../src/core/pwm_bus.h"
#include "arduino/Adafruit_PWMServoDriver.h"
#include "protocol_fixture.h"

namespace TestSupport
{
    inline void RegisterCommandsOnce()
    {
        static bool registered = false;
        if (registered)
            return;

        registered = true;

        protocol.AddHandler(CommandPing::PACKET_TYPE, CommandPing::Handler, nullptr, nullptr);
        protocol.AddHandler(CommandMillis::PACKET_TYPE, CommandMillis::Handler, CommandMillis::Processor, nullptr);
        protocol.AddHandler(CommandVersion::PACKET_TYPE, CommandVersion::Handler, nullptr, nullptr);
        protocol.AddHandler(CommandMotors::PACKET_TYPE, CommandMotors::Handler, nullptr, nullptr);
    }

    class CommandsFixture : public ProtocolFixture
    {
        protected:
        void SetUp() override
        {
            ProtocolFixture::SetUp();
            RegisterCommandsOnce();
            CommandMillis::Reset();
            InitMotors();
            ClearPwmLog();
            channel.Clear();
        }

        void InitMotors()
        {
            PwmBus::Reset();
            CommandMotors::Reset();
            CommandMotors::Init();
        }

        void ClearPwmLog()
        {
            MockPwm::Get().calls.clear();
            MockPwm::Get().channels.clear();
        }
    };
}
