# COM-LINK-RT Protocol
Протокол реального времени для обмена данными по Serial порту.

## Структура пакета

| Смещение | Поле          | Размер  | Описание                |
|----------|---------------|---------|-------------------------|
| 0x00     | syncByte1     | 1 байт  | Первый синхробайт (0xAA)|
| 0x01     | syncByte2     | 1 байт  | Второй синхробайт (0x55)|
| 0x02     | version       | 1 байт  | Версия протокола (0x01) |
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
| 0x05 | DISTANCE - запрос    |
| 0x06 | DISTANCE - ответ     |
| 0x07 | GYRO - запрос        |
| 0x08 | GYRO - ответ         |
| 0x09 | SERVO - запрос       |
| 0x0A | SERVO - ответ        |
| 0x0B | MOTORS - запрос      |
| 0x0C | MOTORS - ответ       |

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

### Стиль кода

Стиль (Allman-скобки, 4 пробела, PascalCase для функций и т.д.) описан в `.clang-format`
и подобран под уже существующий код. Проверка:

```sh
clang-format --dry-run --Werror src/**/*.cpp src/**/*.h com-link-RT.ino
```

Автоформатирование: `clang-format -i <файл>`.

### CI

`.github/workflows/build.yml`:
- `lint` — проверяет форматирование через `clang-format --dry-run --Werror` (блокирующая проверка);
- `build` — собирает прошивку на каждый push/PR и публикует `firmware.bin`/`firmware.elf`
  как артефакт GitHub Actions.
