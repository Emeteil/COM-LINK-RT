#include "motors.h"
#include "../core/globals.h"
#include "../core/pwm_bus.h"
#include <Arduino.h>

namespace CommandMotors
{
    static constexpr uint8_t MOTOR_COUNT = 2;

    static const uint8_t MOTOR_CHANNELS[MOTOR_COUNT] = {0, 1};
    static const uint8_t MOTOR_MASKS[MOTOR_COUNT] = {MOTOR1_MASK, MOTOR2_MASK};

    static constexpr uint16_t PULSE_NEUTRAL = (PWM_SERVO_MIN + PWM_SERVO_MAX) / 2;
    static constexpr uint16_t PULSE_HALF_RANGE = (PWM_SERVO_MAX - PWM_SERVO_MIN) / 2;

    bool initialized;

    MotorState MotorStates[MOTOR_COUNT];

    static inline bool IsValidDirection(uint8_t direction)
    {
        return direction <= DIRECTION_BRAKE;
    }

    static inline bool IsValidIndex(uint8_t motorIndex)
    {
        return motorIndex < MOTOR_COUNT;
    }

    static void ApplyOutput(uint8_t motorIndex)
    {
        if (!initialized)
            return;

        const MotorState& state = MotorStates[motorIndex];

        uint16_t offset = (uint16_t)((uint32_t)state.speed * PULSE_HALF_RANGE / 255);

        uint16_t pulse;
        switch (state.direction)
        {
            case DIRECTION_FORWARD:
                pulse = PULSE_NEUTRAL + offset;
                break;
            case DIRECTION_BACKWARD:
                pulse = PULSE_NEUTRAL - offset;
                break;
            case DIRECTION_STOP:
            case DIRECTION_BRAKE:
            default:
                pulse = PULSE_NEUTRAL;
                break;
        }

        PwmBus::driver.setPWM(MOTOR_CHANNELS[motorIndex], 0, pulse);
    }

    void SetMotorDirection(uint8_t motorIndex, MotorDirection direction)
    {
        if (!IsValidIndex(motorIndex) || !IsValidDirection(direction))
            return;

        MotorStates[motorIndex].direction = direction;
        MotorStates[motorIndex].isActive = (direction != DIRECTION_STOP);

        ApplyOutput(motorIndex);
    }

    void SetMotorSpeed(uint8_t motorIndex, uint8_t speed)
    {
        if (!IsValidIndex(motorIndex))
            return;

        MotorStates[motorIndex].speed = speed;

        ApplyOutput(motorIndex);
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
            length);

        protocol.SendPacket(txBuffer, length, packetId);
    }

    void ProcessMotorCommand(const MotorCommand& cmd)
    {
        const uint8_t speeds[MOTOR_COUNT] = {cmd.speed1, cmd.speed2};
        const uint8_t directions[MOTOR_COUNT] = {cmd.direction1, cmd.direction2};

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
        initialized = PwmBus::Init();
        if (!initialized)
            return;

        for (uint8_t i = 0; i < MOTOR_COUNT; i++)
        {
            MotorStates[i].direction = DIRECTION_STOP;
            MotorStates[i].speed = 0;
            MotorStates[i].isActive = false;
        }
        StopAllMotors();
    }

    void Handler(const ComLinkRTProtocol::PacketHeader& header, const uint8_t* data)
    {
        if (!initialized || header.dataLength != sizeof(MotorCommand))
            return;

        MotorCommand cmd;
        memcpy(&cmd, data, sizeof(MotorCommand));
        ProcessMotorCommand(cmd);

        if (header.packetId != ZERO_PACKET_ID)
            SendResponse(header.packetId);
    }
}
