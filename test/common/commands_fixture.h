#pragma once
#include <Arduino.h>
#include "../../src/commands/distance.h"
#include "../../src/commands/gps.h"
#include "../../src/commands/magnetometer.h"
#include "../../src/commands/millis.h"
#include "../../src/commands/motors.h"
#include "../../src/commands/ping.h"
#include "../../src/commands/version.h"
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
        protocol.AddHandler(CommandDistance::PACKET_TYPE, CommandDistance::Handler, CommandDistance::Processor, nullptr);
        protocol.AddHandler(CommandMagnetometer::PACKET_TYPE, CommandMagnetometer::Handler, CommandMagnetometer::Processor, nullptr);
        protocol.AddHandler(CommandGps::PACKET_TYPE, CommandGps::Handler, CommandGps::Processor, nullptr);
    }

    class CommandsFixture : public ProtocolFixture
    {
        protected:
        void SetUp() override
        {
            ProtocolFixture::SetUp();
            RegisterCommandsOnce();
            CommandMillis::Reset();
            CommandDistance::Init();
            CommandMagnetometer::Init();
            CommandGps::Init();
            InitMotors();
            ClearPinLog();
            channel.Clear();
        }

        void InitMotors()
        {
            CommandMotors::Reset();
            CommandMotors::Init();
        }

        void ClearPinLog()
        {
            MockArduino::Get().pinModes.clear();
            MockArduino::Get().digitalWrites.clear();
            MockArduino::Get().analogWrites.clear();
        }
    };
}
