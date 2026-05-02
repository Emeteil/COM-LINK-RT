#include "motors.h"
#include "../core/globals.h"
#include <Arduino.h>

namespace CommandMotors
{
    static constexpr uint8_t MOTOR_COUNT = 2;

    struct MotorPins
    {
        uint8_t ena;
        uint8_t in1;
        uint8_t in2;
    };

    static const MotorPins MOTOR_PINS[MOTOR_COUNT] = {
        {MOTOR1_ENA_PIN, MOTOR1_IN1_PIN, MOTOR1_IN2_PIN},
        {MOTOR2_ENA_PIN, MOTOR2_IN1_PIN, MOTOR2_IN2_PIN},
    };

    static const uint8_t MOTOR_MASKS[MOTOR_COUNT] = {MOTOR1_MASK, MOTOR2_MASK};

    static const uint8_t DIRECTION_LEVELS[4][2] = {
        {LOW,  LOW},   // DIRECTION_STOP
        {HIGH, LOW},   // DIRECTION_FORWARD
        {LOW,  HIGH},  // DIRECTION_BACKWARD
        {HIGH, HIGH},  // DIRECTION_BRAKE
    };

    MotorState MotorStates[MOTOR_COUNT];

    static inline bool IsValidDirection(uint8_t direction)
    {
        return direction <= DIRECTION_BRAKE;
    }

    static inline bool IsValidIndex(uint8_t motorIndex)
    {
        return motorIndex < MOTOR_COUNT;
    }

    void SetMotorDirection(uint8_t motorIndex, MotorDirection direction)
    {
        if (!IsValidIndex(motorIndex) || !IsValidDirection(direction))
            return;

        const MotorPins& pins = MOTOR_PINS[motorIndex];
        const uint8_t* levels = DIRECTION_LEVELS[direction];

        digitalWrite(pins.in1, levels[0]);
        digitalWrite(pins.in2, levels[1]);

        MotorStates[motorIndex].direction = direction;
        MotorStates[motorIndex].isActive = (direction != DIRECTION_STOP);
    }

    void SetMotorSpeed(uint8_t motorIndex, uint8_t speed)
    {
        if (!IsValidIndex(motorIndex))
            return;

        analogWrite(MOTOR_PINS[motorIndex].ena, speed);
        MotorStates[motorIndex].speed = speed;
    }

    static inline void ApplyDirectionIfMasked(uint8_t motorMask, uint8_t motorIndex, uint8_t direction)
    {
        if (motorMask & MOTOR_MASKS[motorIndex])
            SetMotorDirection(motorIndex, static_cast<MotorDirection>(direction));
    }

    static inline void ApplySpeedIfMasked(uint8_t motorMask, uint8_t motorIndex, uint8_t speed)
    {
        if (motorMask & MOTOR_MASKS[motorIndex])
            SetMotorSpeed(motorIndex, speed);
    }

    void StopAllMotors()
    {
        for (uint8_t i = 0; i < MOTOR_COUNT; i++)
        {
            SetMotorDirection(i, DIRECTION_STOP);
            SetMotorSpeed(i, 0);
        }
    }

    void SendResponse(uint16_t packetId)
    {
        uint8_t txBuffer[64];
        uint16_t length;

        protocol.parser.CreatePacket(
            PROTOCOL_VERSION,
            PACKET_TYPE_RESPONSE,
            EMPTY_SERVICE_BITS,
            packetId,
            nullptr, 0,
            txBuffer,
            length
        );

        protocol.SendPacket(txBuffer, length, packetId);
    }

    void ProcessMotorCommand(const MotorCommand& cmd)
    {
        const uint8_t speeds[MOTOR_COUNT]      = {cmd.speed1, cmd.speed2};
        const uint8_t directions[MOTOR_COUNT]  = {cmd.direction1, cmd.direction2};

        switch (cmd.commandType)
        {
            case COMMAND_SET_SPEED:
                for (uint8_t i = 0; i < MOTOR_COUNT; i++)
                    ApplySpeedIfMasked(cmd.motorMask, i, speeds[i]);
                break;

            case COMMAND_SET_DIRECTION:
                for (uint8_t i = 0; i < MOTOR_COUNT; i++)
                    ApplyDirectionIfMasked(cmd.motorMask, i, directions[i]);
                break;

            case COMMAND_SET_BOTH:
            case COMMAND_SET_DIFFERENTIAL:
                for (uint8_t i = 0; i < MOTOR_COUNT; i++)
                {
                    const uint8_t mask = (cmd.commandType == COMMAND_SET_DIFFERENTIAL) ? 0xFF : cmd.motorMask;
                    ApplyDirectionIfMasked(mask, i, directions[i]);
                    ApplySpeedIfMasked(mask, i, speeds[i]);
                }
                break;

            case COMMAND_STOP_ALL:
                StopAllMotors();
                break;
        }
    }

    void Init()
    {
        for (uint8_t i = 0; i < MOTOR_COUNT; i++)
        {
            pinMode(MOTOR_PINS[i].ena, OUTPUT);
            pinMode(MOTOR_PINS[i].in1, OUTPUT);
            pinMode(MOTOR_PINS[i].in2, OUTPUT);
            MotorStates[i].direction = DIRECTION_STOP;
            MotorStates[i].speed = 0;
            MotorStates[i].isActive = false;
        }
        StopAllMotors();
    }

    void Handler(ComLinkRTProtocol::PacketHeader header, uint8_t* data)
    {
        if (header.dataLength != sizeof(MotorCommand))
            return;

        MotorCommand cmd;
        memcpy(&cmd, data, sizeof(MotorCommand));
        ProcessMotorCommand(cmd);

        if (header.packetId != ZERO_PACKET_ID)
            SendResponse(header.packetId);
    }
}
