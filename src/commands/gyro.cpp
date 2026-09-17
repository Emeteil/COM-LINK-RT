#include "gyro.h"
#include "../core/subscription_base.h"
#include "../core/globals.h"
#include <Arduino.h>
#include <Wire.h>
#include <MPU6050.h>

namespace CommandGyro
{
    MPU6050 mpu;

    class GyroHandler : public CommandSubscription::SubscriptionHandler
    {
        private:
        enum CalibrationState
        {
            CALIBRATION_IDLE,
            CALIBRATION_RUNNING,
            CALIBRATION_COMPLETE
        };

        CalibrationState calState;
        unsigned long calibrationStartTime;
        unsigned long lastCalibrationSampleTime;
        int calibrationSamples;
        int32_t sumAccelX, sumAccelY, sumAccelZ;
        int32_t sumGyroX, sumGyroY, sumGyroZ;

        bool calibrated;
        int16_t accelOffsetX, accelOffsetY, accelOffsetZ;
        int16_t gyroOffsetX, gyroOffsetY, gyroOffsetZ;

        GyroData filteredData;

        protected:
        void SendResponse(uint8_t packetType, uint16_t packetId, uint8_t serviceBits) override
        {
            uint8_t txBuffer[64];
            uint16_t length;

            GyroData gyroData;

            mpu.getMotion6(&gyroData.accelX, &gyroData.accelY, &gyroData.accelZ, &gyroData.gyroX, &gyroData.gyroY, &gyroData.gyroZ);
            gyroData.temperature = mpu.getTemperature();

            uint8_t gyroServiceBits = (serviceBits & 0xF0) | (headerServiceBits & 0x0F);

            if (gyroServiceBits & GYRO_SERVICE_BITS_CALIBRATED && calibrated)
            {
                gyroData.accelX -= accelOffsetX;
                gyroData.accelY -= accelOffsetY;
                gyroData.accelZ -= accelOffsetZ;
                gyroData.gyroX -= gyroOffsetX;
                gyroData.gyroY -= gyroOffsetY;
                gyroData.gyroZ -= gyroOffsetZ;
            }

            if (gyroServiceBits & GYRO_SERVICE_BITS_FILTERED)
            {
                const float alpha = 0.3;

                filteredData.accelX = filteredData.accelX * (1 - alpha) + gyroData.accelX * alpha;
                filteredData.accelY = filteredData.accelY * (1 - alpha) + gyroData.accelY * alpha;
                filteredData.accelZ = filteredData.accelZ * (1 - alpha) + gyroData.accelZ * alpha;
                filteredData.gyroX = filteredData.gyroX * (1 - alpha) + gyroData.gyroX * alpha;
                filteredData.gyroY = filteredData.gyroY * (1 - alpha) + gyroData.gyroY * alpha;
                filteredData.gyroZ = filteredData.gyroZ * (1 - alpha) + gyroData.gyroZ * alpha;

                gyroData = filteredData;
            }

            protocol.parser.CreatePacket(
                PROTOCOL_VERSION,
                packetType,
                serviceBits,
                packetId,
                reinterpret_cast<uint8_t*>(&gyroData),
                sizeof(gyroData),
                txBuffer,
                length);

            protocol.SendPacket(txBuffer, length, packetId);
        }

        uint8_t headerServiceBits;

        void ProcessCalibration()
        {
            unsigned long currentTime = millis();

            switch (calState)
            {
                case CALIBRATION_IDLE:
                    break;

                case CALIBRATION_RUNNING:
                    if (currentTime - lastCalibrationSampleTime >= 10)
                    {
                        int16_t ax, ay, az, gx, gy, gz;
                        mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

                        sumAccelX += ax;
                        sumAccelY += ay;
                        sumAccelZ += az;
                        sumGyroX += gx;
                        sumGyroY += gy;
                        sumGyroZ += gz;

                        calibrationSamples++;
                        lastCalibrationSampleTime = currentTime;

                        if (calibrationSamples >= 100)
                        {
                            accelOffsetX = sumAccelX / calibrationSamples;
                            accelOffsetY = sumAccelY / calibrationSamples;
                            accelOffsetZ = (sumAccelZ / calibrationSamples) - 16384;
                            gyroOffsetX = sumGyroX / calibrationSamples;
                            gyroOffsetY = sumGyroY / calibrationSamples;
                            gyroOffsetZ = sumGyroZ / calibrationSamples;

                            calibrated = true;
                            calState = CALIBRATION_COMPLETE;
                        }
                    }
                    break;

                case CALIBRATION_COMPLETE:
                    calState = CALIBRATION_IDLE;
                    break;
            }
        }

        public:
        GyroHandler() : SubscriptionHandler(50, 5000, PACKET_TYPE_RESPONSE), calState(CALIBRATION_IDLE), calibrated(false), headerServiceBits(0)
        {
            filteredData = {0, 0, 0, 0, 0, 0, 0};
            accelOffsetX = accelOffsetY = accelOffsetZ = 0;
            gyroOffsetX = gyroOffsetY = gyroOffsetZ = 0;
        }

        void HandleSubscription(ComLinkRTProtocol::PacketHeader header, uint8_t* data)
        {
            headerServiceBits = header.serviceBits & 0x0F;

            if ((header.serviceBits & 0x0F) == 0x01 && calState == CALIBRATION_IDLE)
            {
                StartCalibration();
                return;
            }

            SubscriptionHandler::HandleSubscription(header, data);
        }

        void StartCalibration()
        {
            calState = CALIBRATION_RUNNING;
            calibrationStartTime = millis();
            lastCalibrationSampleTime = calibrationStartTime;
            calibrationSamples = 0;
            sumAccelX = sumAccelY = sumAccelZ = 0;
            sumGyroX = sumGyroY = sumGyroZ = 0;
            calibrated = false;
        }

        void Process()
        {
            ProcessCalibration();
            ProcessSubscription();
        }

        bool IsCalibrating() const { return calState == CALIBRATION_RUNNING; }
        bool IsCalibrated() const { return calibrated; }
    };

    GyroHandler gyroHandler;

    void Init()
    {
        Wire.setSDA(I2C1_SDA);
        Wire.setSCL(I2C1_SCL);
        Wire.begin();
        mpu.initialize();

        if (mpu.testConnection())
        {
            mpu.setFullScaleAccelRange(MPU6050_ACCEL_FS_2);
            mpu.setFullScaleGyroRange(MPU6050_GYRO_FS_250);
            mpu.setDLPFMode(MPU6050_DLPF_BW_42);

            gyroHandler.StartCalibration();
        }
    }

    void Handler(ComLinkRTProtocol::PacketHeader header, uint8_t* data)
    {
        gyroHandler.HandleSubscription(header, data);
    }

    void Processor()
    {
        gyroHandler.Process();
    }
}