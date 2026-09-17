#pragma once
#include "../protocol/com_protocol.h"
#include "../core/i2c_settings.h"

namespace CommandGyro
{
    const uint8_t PACKET_TYPE_REQUEST = 0x07;
    const uint8_t PACKET_TYPE_RESPONSE = 0x08;
    const uint8_t PACKET_TYPE = PACKET_TYPE_REQUEST;

    const uint8_t GYRO_SERVICE_BITS_RAW = 0x00;
    const uint8_t GYRO_SERVICE_BITS_FILTERED = 0x01;
    const uint8_t GYRO_SERVICE_BITS_CALIBRATED = 0x02;

    struct GyroData
    {
        int16_t accelX;
        int16_t accelY;
        int16_t accelZ;
        int16_t gyroX;
        int16_t gyroY;
        int16_t gyroZ;
        int16_t temperature;
    };

    void Handler(ComLinkRTProtocol::PacketHeader header, uint8_t* data);
    void Processor();
    void Init();
}