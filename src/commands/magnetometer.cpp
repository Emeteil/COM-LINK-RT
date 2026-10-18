#include "magnetometer.h"
#include "../core/subscription_base.h"
#include "../core/globals.h"
#include <Arduino.h>
#include <Wire.h>

namespace CommandMagnetometer
{
    static constexpr uint8_t REG_CHIP_ID = 0x00;
    static constexpr uint8_t REG_DATA_X_LSB = 0x01;
    static constexpr uint8_t REG_STATUS = 0x09;
    static constexpr uint8_t REG_CONTROL_1 = 0x0A;
    static constexpr uint8_t REG_CONTROL_2 = 0x0B;
    static constexpr uint8_t REG_AXIS_SIGN = 0x29;

    static constexpr uint8_t CHIP_ID = 0x80;
    static constexpr uint8_t AXIS_SIGN_DEFAULT = 0x06;
    static constexpr uint8_t CONTROL_2_SET_RESET_8G = 0x08;
    static constexpr uint8_t CONTROL_1_NORMAL_50HZ = 0xC5;

    static constexpr uint8_t STATUS_BIT_DRDY = 0x01;
    static constexpr uint8_t STATUS_BIT_OVERFLOW = 0x02;
    static constexpr uint8_t MAX_ERRORS = 3;

    static const uint8_t ADDRESSES[] = {MAGNETOMETER_I2C_ADDRESS, MAGNETOMETER_I2C_ADDRESS_ALT};

    static MagnetometerData current;
    static uint8_t address = MAGNETOMETER_I2C_ADDRESS;
    static bool busStarted = false;
    static bool present = false;
    static uint8_t errorCount = 0;
    static unsigned long lastProbeMs = 0;
    static unsigned long lastReadMs = 0;

    static bool StartBus();

    static void StopBus()
    {
        if (!busStarted)
            return;
        Wire.end();
        busStarted = false;
    }

    static bool Failed()
    {
        StopBus();
        return false;
    }

    static bool WriteRegister(uint8_t reg, uint8_t value)
    {
        if (!StartBus())
            return false;
        Wire.beginTransmission(address);
        Wire.write(reg);
        Wire.write(value);
        return Wire.endTransmission() == 0 || Failed();
    }

    static bool ReadRegisters(uint8_t reg, uint8_t* out, uint8_t count)
    {
        if (!StartBus())
            return false;
        Wire.beginTransmission(address);
        Wire.write(reg);
        if (Wire.endTransmission() != 0)
            return Failed();
        if (Wire.requestFrom(address, count) != count)
            return Failed();
        for (uint8_t i = 0; i < count; i++)
            out[i] = static_cast<uint8_t>(Wire.read());
        return true;
    }

    static bool BusIdle()
    {
        return digitalRead(I2C1_SCL) == HIGH && digitalRead(I2C1_SDA) == HIGH;
    }

    static bool StartBus()
    {
        if (busStarted)
            return true;

        pinMode(I2C1_SCL, INPUT);
        pinMode(I2C1_SDA, INPUT);
        if (!BusIdle())
            return false;

        Wire.setSDA(I2C1_SDA);
        Wire.setSCL(I2C1_SCL);
        Wire.begin();
        Wire.setClock(MAGNETOMETER_I2C_CLOCK_HZ);
        busStarted = true;
        return true;
    }

    static void Probe()
    {
        if (!StartBus() || !BusIdle())
        {
            current.status = STATUS_BUS_STUCK;
            return;
        }

        bool found = false;
        for (uint8_t i = 0; i < sizeof(ADDRESSES) && !found; i++)
        {
            address = ADDRESSES[i];
            uint8_t chipId = 0;
            found = ReadRegisters(REG_CHIP_ID, &chipId, 1) && chipId == CHIP_ID;
        }
        if (!found)
        {
            current.status = STATUS_NOT_FOUND;
            return;
        }

        if (!WriteRegister(REG_AXIS_SIGN, AXIS_SIGN_DEFAULT) ||
            !WriteRegister(REG_CONTROL_2, CONTROL_2_SET_RESET_8G) ||
            !WriteRegister(REG_CONTROL_1, CONTROL_1_NORMAL_50HZ))
        {
            current.status = STATUS_NOT_FOUND;
            return;
        }

        present = true;
        errorCount = 0;
        current.status = STATUS_NO_DATA;
    }

    static void OnReadError()
    {
        if (++errorCount < MAX_ERRORS)
            return;
        present = false;
        lastProbeMs = millis();
        current.status = STATUS_NOT_FOUND;
    }

    static void ReadSample()
    {
        uint8_t status = 0;
        if (!ReadRegisters(REG_STATUS, &status, 1))
        {
            OnReadError();
            return;
        }
        if (!(status & STATUS_BIT_DRDY))
            return;

        uint8_t raw[6];
        if (!ReadRegisters(REG_DATA_X_LSB, raw, sizeof(raw)))
        {
            OnReadError();
            return;
        }

        errorCount = 0;
        current.x = static_cast<int16_t>(raw[0] | (raw[1] << 8));
        current.y = static_cast<int16_t>(raw[2] | (raw[3] << 8));
        current.z = static_cast<int16_t>(raw[4] | (raw[5] << 8));
        current.status = (status & STATUS_BIT_OVERFLOW) ? STATUS_OVERFLOW : STATUS_OK;
    }

    static void TickSensor()
    {
        const unsigned long now = millis();

        if (!present)
        {
            if (now - lastProbeMs >= MAGNETOMETER_PROBE_INTERVAL_MS)
            {
                lastProbeMs = now;
                Probe();
            }
            return;
        }

        if (now - lastReadMs >= MAGNETOMETER_READ_INTERVAL_MS)
        {
            lastReadMs = now;
            ReadSample();
        }
    }

    class MagnetometerHandler : public CommandSubscription::SubscriptionHandler
    {
        protected:
        void SendResponse(uint8_t packetType, uint16_t packetId, uint8_t serviceBits) override
        {
            uint8_t txBuffer[64];
            uint16_t length;

            protocol.parser.CreatePacket(
                PROTOCOL_VERSION,
                packetType,
                serviceBits,
                packetId,
                reinterpret_cast<const uint8_t*>(&current),
                sizeof(current),
                txBuffer,
                length);

            protocol.SendPacket(txBuffer, length, packetId);
        }

        public:
        MagnetometerHandler() : SubscriptionHandler(MAGNETOMETER_SEND_INTERVAL_MS, SUBSCRIPTION_KEEP_ALIVE_TIMEOUT_MS, PACKET_TYPE_RESPONSE) {}
    };

    MagnetometerHandler magnetometerHandler;

    void Reset()
    {
        magnetometerHandler.ResetSubscription();
        current.x = current.y = current.z = 0;
        current.status = STATUS_NOT_FOUND;
        busStarted = false;
        present = false;
        errorCount = 0;
        lastProbeMs = millis();
        lastReadMs = millis();
    }

    void Init()
    {
        Reset();
        Probe();
    }

    void Handler(const ComLinkRTProtocol::PacketHeader& header, const uint8_t* data)
    {
        magnetometerHandler.HandleSubscription(header, data);
    }

    void Processor()
    {
        TickSensor();
        magnetometerHandler.ProcessSubscription();
    }
}
