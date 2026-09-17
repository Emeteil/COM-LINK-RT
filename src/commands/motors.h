#pragma once
#include "../protocol/com_protocol.h"
#include "../core/board_config.h"

namespace CommandMotors
{
    const uint8_t PACKET_TYPE_REQUEST = 0x0B;
    const uint8_t PACKET_TYPE_RESPONSE = 0x0C;
    const uint8_t PACKET_TYPE = PACKET_TYPE_REQUEST;

    enum MotorDirection : uint8_t
    {
        DIRECTION_STOP = 0x00,
        DIRECTION_FORWARD = 0x01,
        DIRECTION_BACKWARD = 0x02,
        DIRECTION_BRAKE = 0x03
    };

    enum MotorCommandType : uint8_t
    {
        COMMAND_SET_SPEED = 0x01,
        COMMAND_SET_DIRECTION = 0x02,
        COMMAND_SET_BOTH = 0x03,
        COMMAND_STOP_ALL = 0x04,
        COMMAND_SET_DIFFERENTIAL = 0x05
    };

#pragma pack(push, 1)
    struct MotorCommand
    {
        MotorCommandType commandType;
        uint8_t motorMask;  // Бит 0: мотор 1, бит 1: мотор 2 [0x01 и 0x02 или 0x03 для обоих моторов]
        uint8_t direction1; // Направление мотора 1 (MotorDirection)
        uint8_t direction2; // Направление мотора 2 (MotorDirection)
        uint8_t speed1;     // Скорость мотора 1 (0-255)
        uint8_t speed2;     // Скорость мотора 2 (0-255)
    };
#pragma pack(pop)

    struct MotorState
    {
        MotorDirection direction;
        uint8_t speed;
        bool isActive;
    };

    void Init();
    void Handler(const ComLinkRTProtocol::PacketHeader& header, const uint8_t* data);
}