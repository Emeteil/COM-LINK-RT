#pragma once
#include "../protocol/com_protocol.h"
#include "../core/i2c_settings.h"

#define MAX_CHANNELS 16
#define MAX_TASKS 10
#define SERVO_MIN 150
#define SERVO_MAX 600
#define SERVO_FREQ 50

#define SERVO_DISABLE_SMOOTH

namespace CommandServo
{
    const uint8_t PACKET_TYPE_REQUEST = 0x09;
    const uint8_t PACKET_TYPE_RESPONSE = 0x0A;
    const uint8_t PACKET_TYPE = PACKET_TYPE_REQUEST;

    enum ServoMode : uint8_t
    {
        MODE_ABSOLUTE_POSITION = 0x01, // Абсолютная позиция
        MODE_RELATIVE_POSITION = 0x02, // Относительная позиция
        MODE_CALIBRATION = 0x03 // Калибровка
    };

    enum TaskPriority : uint8_t
    {
        PRIORITY_LOW = 0x01, // Низкий приоритет (в конец очереди)
        PRIORITY_HIGH = 0x03 // Высокий приоритет (очищает очередь)
    };

    enum MovementType : uint8_t
    {
        MOVE_IMMEDIATE = 0x01, // Резкий поворот (высший приоритет)
        MOVE_SMOOTH_LOW = 0x02, // Плавный поворот (низкий приоритет)
        MOVE_SMOOTH_HIGH = 0x03 // Плавный поворот (высокий приоритет)
    };

    #pragma pack(push, 1)
    struct ServoCommand
    {
        uint8_t channel; // Канал сервопривода
        uint8_t moveType; // Тип движения (MovementType)
        uint16_t targetAngle; // Целевой угол (0-180 градусов или относительное значение)
        uint16_t stepDelay; // Задержка между шагами в мс (для плавного движения)
        uint8_t mode; // Режим работы (ServoMode)
    };
    #pragma pack(pop)

    struct ServoTask
    {
        ServoCommand command;
        unsigned long startTime;
        unsigned long lastStepTime;
        uint16_t currentAngle;
        uint16_t startAngle;
        bool isActive;
        TaskPriority priority;
    };
    
    struct ChannelState
    {
        uint16_t currentAngle;
        bool isDefined;
    };

    void Handler(ComLinkRTProtocol::PacketHeader header, uint8_t* data);
    void Processor();
    void Init();
}