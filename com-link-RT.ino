/*
 * COM-LINK-RT Protocol
 * ====================
 * Протокол реального времени для обмена данными по Serial порту.
 * Предназначен для устройств STM32 и других микроконтроллеров,
 * совместимых со средой Arduino.
 *
 * Структура пакета (Little-Endian):
 * ┌────────┬────────┬─────────┬──────────┬─────────────┬──────────┬────────────┬───────┐
 * │ Синхро │ Синхро │ Версия  │   Тип    |  Служебные  |    ID    |   Длина    |  CRC  │
 * │ байт 1 │ байт 2 │ (0x01)  │  пакета  |    биты     │  пакета  │   данных   │       │
 * │ (0xAA) │ (0x55) │         │          |             |          |            |       │
 * ├────────┼────────┼─────────┼──────────┼─────────────┼──────────┼────────────┼───────┤
 * │ 1 байт │ 1 байт │ 1 байт  │  1 байт  │   1 байт    │ 2 байта  │  2 байта   │2 байта│
 * └────────┴────────┴─────────┴──────────┴─────────────┴──────────┴────────────┴───────┘
 *
 * Версия протокола: 0x01
 * Заголовок: 11 байт.
 * После заголовка могут следовать данные длиной `dataLength` байт.
 *
 * Зарезервированные типы пакетов:
 * 0x01, 0x02 - Команда PING (запрос/ответ)
 * 0x03, 0x04 - Команда MILLIS (запрос/ответ) с поддержкой подписки
 * 0x05, 0x06 - Команда DISTANCE (запрос/ответ) с поддержкой подписки
 * 0x07, 0x08 - Команда GYRO (запрос/ответ) с поддержкой подписки и калибровки
 * 0x09, 0x0A - Команда SERVO (запрос/ответ)
 * 0x0B, 0x0C - Команда MOTORS (запрос/ответ)
 *
 * Служебные биты (для MILLIS):
 * 0x80 (10000000) - SUBSCRIBE    (Подписаться на поток данных)
 * 0x40 (01000000) - UNSUBSCRIBED (Подтверждение отписки)
 * 0x20 (00100000) - KEEP_ALIVE   (Поддержание подписки)
 * 0x10 (00010000) - UNSUBSCRIBE  (Запрос на отписку)
 */
#include "src/core/globals.h"
#include "src/protocol/com_serial_channel.h"

#include "src/commands/ping.h"
#include "src/commands/millis.h"
#include "src/commands/distance.h"
#include "src/commands/gyro.h"
#include "src/commands/servo.h"
#include "src/commands/motors.h"

ComLinkRTProtocol::SerialChannel serial1Channel(Serial1, READING_SPEED);
ComLinkRTProtocol::ProtocolHandler protocol(serial1Channel);

void setup()
{
    protocol.AddHandler(CommandPing::PACKET_TYPE, CommandPing::Handler, nullptr, nullptr);
    protocol.AddHandler(CommandMillis::PACKET_TYPE, CommandMillis::Handler, CommandMillis::Processor, nullptr);
    protocol.AddHandler(CommandDistance::PACKET_TYPE, CommandDistance::Handler, CommandDistance::Processor, CommandDistance::Init);
    protocol.AddHandler(CommandServo::PACKET_TYPE, CommandServo::Handler, CommandServo::Processor, CommandServo::Init);
    protocol.AddHandler(CommandMotors::PACKET_TYPE, CommandMotors::Handler, nullptr, CommandMotors::Init);
    // protocol.AddHandler(CommandGyro::PACKET_TYPE, CommandGyro::Handler, CommandGyro::Processor, CommandGyro::Init);

    protocol.Begin();
}

void loop()
{
    protocol.Update();
}