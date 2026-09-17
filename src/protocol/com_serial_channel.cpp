#include "com_serial_channel.h"
#include <Arduino.h>

namespace ComLinkRTProtocol
{
    SerialChannel::SerialChannel(HardwareSerial& serial, unsigned long baudRate) : serial(serial), baudRate(baudRate)
    {
    }

    void SerialChannel::Begin()
    {
        serial.begin(baudRate);
    }

    int SerialChannel::Available()
    {
        return serial.available();
    }

    uint8_t SerialChannel::Read()
    {
        return static_cast<uint8_t>(serial.read());
    }

    void SerialChannel::Write(const uint8_t* data, uint16_t length)
    {
        serial.write(data, length);
    }
}
