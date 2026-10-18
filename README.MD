# COM-LINK-RT Protocol
Протокол реального времени для обмена данными по Serial порту.

## Структура пакета

| Смещение | Поле          | Размер  | Описание                |
|----------|---------------|---------|-------------------------|
| 0x00     | syncByte1     | 1 байт  | Первый синхробайт (0xAA)|
| 0x01     | syncByte2     | 1 байт  | Второй синхробайт (0x55)|
| 0x02     | version       | 1 байт  | Версия протокола (0x03) |
| 0x03     | packetType    | 1 байт  | Тип пакета              |
| 0x04     | serviceBits   | 1 байт  | Служебные биты          |
| 0x05     | packetId      | 2 байта | Идентификатор пакета    |
| 0x07     | dataLength    | 2 байта | Длина данных            |
| 0x09     | crc           | 2 байта | Контрольная сумма       |
| 0x0B     | data          | N байт  | Данные (опционально)    |

**Размер заголовка:** 11 байт

## Зарезервированные типы пакетов

| Тип | Назначение           |
|-----|----------------------|
| 0x01 | PING - запрос        |
| 0x02 | PING - ответ         |
| 0x03 | MILLIS - запрос      |
| 0x04 | MILLIS - ответ       |
| 0x05 | VERSION - запрос     |
| 0x06 | VERSION - ответ      |
| 0x07 | MOTORS - запрос      |
| 0x08 | MOTORS - ответ       |
| 0x09 | DISTANCE - запрос    |
| 0x0A | DISTANCE - ответ     |

### MOTORS

Два мотора через H-мост (L298N): `EN` — ШИМ скорости (PE10, PE13), `IN1`/`IN2` — направление
(PE11/PE12, PE14/PE15). Payload запроса — 6 байт `MotorCommand` (`motors.h`), ответ пустой.

### DISTANCE

8 датчиков HC-SR04 через два CD74HC4067 (TRIG — PC1, ECHO — PC2, адрес S0–S2 — PD12–PD14).
Датчики опрашиваются по очереди 0, 4, 1, 5, 2, 6, 3, 7 фиксированными слотами по 31 мс
(круг 248 мс), без блокировок: фронты эха ловятся прерыванием, длительность считается
по счётчику тактов ядра (DWT), основной цикл только переключает слоты.

Ответ — `8 × uint16` little-endian, миллиметры, индекс = номер датчика (канал `Cn`):

| Значение | Смысл                                         |
|----------|-----------------------------------------------|
| `1…65534`| расстояние в мм                               |
| `0`      | эхо не вернулось в пределах дальности (~4 м)  |
| `65535`  | датчик не ответил или ещё не измерялся        |

Одиночный запрос возвращает последние значения. С подпиской (`SUBSCRIBE`) значения
приходят раз в 250 мс (~4 Гц), каждый пакет — самые свежие замеры всех датчиков.
Скорость звука задана для 20 °C (`ULTRASONIC_SOUND_SPEED_MM_S` в `board_config.h`).

## Служебные биты (для подписок)

| Биты | Назначение       | Описание                |
|------|------------------|-------------------------|
| 0x80 | SUBSCRIBE        | Подписка на поток       |
| 0x40 | UNSUBSCRIBED     | Подтверждение отписки   |
| 0x20 | KEEP_ALIVE       | Поддержание подписки    |
| 0x10 | UNSUBSCRIBE      | Запрос на отписку       |

## Сборка и прошивка без Arduino IDE

Проект собирается через [PlatformIO](https://platformio.org/) (плата `black_f407ve`,
`STM32F407VET6`). PlatformIO кроссплатформенный (Linux/Windows/macOS), ставится через
`pip install platformio`.

### VS Code

Установите расширение PlatformIO IDE — проект (`platformio.ini`) откроется и соберётся
автоматически, кнопки Build/Upload/Monitor появятся в нижней панели.

### CLI / just

```sh
pio run              # сборка (env по умолчанию: black_f407ve)
pio run -t upload    # прошивка по SWD через ST-Link (OpenOCD, без STM32CubeProgrammer)
pio device monitor -b 115200

# или через just (https://github.com/casey/just):
just build
just upload
just upload-dfu                 # прошивка через встроенный USB DFU-бутлоадер (BOOT0 -> 1)
just upload-serial COM9         # прошивка через USB-UART переходник (BOOT0 -> 1)
```

Все три способа прошивки (ST-Link/SWD через OpenOCD, USB DFU, UART-бутлоадер) не требуют
STM32CubeProgrammer — PlatformIO сам тянет нужные инструменты (OpenOCD, dfu-util, stm32flash).

Если путь к проекту содержит не-ASCII символы (кириллицу и т.п.) — на Windows это может
ломать линковку (`ld.exe: cannot open map file`). Скопируйте `platformio_local.ini.example`
в `platformio_local.ini` (в .gitignore) и укажите там `build_dir` с ASCII-путём.

### Тесты

Unit-тесты нативные (GoogleTest), железо не нужно:

```sh
just test            # или: pio test -e native
just test-core       # только ядро протокола
just test-commands   # только команды
```

Тесты лежат в `test/`, в прошивку не собираются. Новый набор — новая папка `test/test_<name>/`
с `.cpp`-файлом. `Arduino.h` и `Wire.h` подменены заглушками
из `test/common/arduino/` (время управляется из теста через `MockArduino::Advance()`),
общие хелперы — в `test/common/`.

#### Тест для новой команды

1. Зарегистрируйте команду в `RegisterCommandsOnce()` в `test/common/commands_fixture.h`
   (так же, как в `setup()` в `com-link-RT.ino`) и подключите её заголовок.
2. Если у команды есть глобальное состояние — добавьте `void Reset()` в её namespace
   и вызовите его в `CommandsFixture::SetUp()`.
3. Создайте `test/test_commands/test_foo.cpp`:

```cpp
#include <gtest/gtest.h>
#include "commands_fixture.h"

using TestSupport::CommandsFixture;
using TestSupport::ExpectPacket;
using TestSupport::PacketBuilder;

TEST_F(CommandsFixture, FooAnswersWithSameId)
{
    Exchange(PacketBuilder(CommandFoo::PACKET_TYPE, 0x0042).Payload({0x01, 0x02}));

    ASSERT_EQ(SentCount(), 1u);
    ExpectPacket(LastSent(), CommandFoo::PACKET_TYPE_RESPONSE, 0x0042);
    EXPECT_EQ(LastSent().payload.size(), 2u);
}
```

Полезно знать: `Exchange(пакет)` отправляет пакет и обрабатывает его, `Tick(мс)` двигает время
и вызывает процессоры (подписки), `PacketBuilder` умеет `.ServiceBits()`, `.BreakCrc()`,
`.Version()`, `.DeclaredLength()`, а вызовы `pinMode`/`digitalWrite`/`analogWrite` можно проверить
через `MockArduino::LastDigitalWrite(pin)` и `MockArduino::LastAnalogWrite(pin)`.
`MockArduino::DriveInterruptPin(pin, уровень)` выставляет уровень и вызывает обработчик из `attachInterrupt`.

### Стиль кода

Стиль (Allman-скобки, 4 пробела, PascalCase для функций и т.д.) описан в `.clang-format`
и подобран под уже существующий код. Проверка:

```sh
clang-format --dry-run --Werror src/**/*.cpp src/**/*.h com-link-RT.ino
```

Автоформатирование: `clang-format -i <файл>`.

### CI

На каждый push/PR:
- `lint` (`.github/workflows/lint.yml`) — проверяет форматирование через
  `clang-format --dry-run --Werror` (блокирующая проверка);
- `build` (`.github/workflows/build.yml`) — собирает прошивку и публикует `firmware.bin`/`firmware.elf`
  как артефакт GitHub Actions;
- `native-tests` (`.github/workflows/test.yml`) — прогоняет `pio test -e native`.
