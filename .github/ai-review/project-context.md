# Project context
`com-link-RT` (COM-LINK-RT) — прошивка C++ (Arduino Core) для управляющего микроконтроллера STM32 робота «Избушка»: приём/разбор бинарного протокола, управление моторами, серво и датчиками в жёстком реальном времени.

## What to review and what to ignore
Ревьюить: `com-link-RT.ino`, всё содержимое `src/` (парсер протокола, обработчики команд, драйверы датчиков/моторов).
Игнорировать: `.github/`, CI-конфиги, `.gitignore`, сгенерированные/бинарные файлы сборки (если появятся).
Если в диффе нет ревьюабельного кода — так и напиши в summary, не выдумывай замечания.

## Always read the PR description and comments
Перед ревью прочитай PR DESCRIPTION из PR / GIT CONTEXT и комментарии в pr-comments/others/. Не поднимай повторно то, что там уже решено или объяснено; объяснение снимает придирку, но не отменяет реальный баг.

## Stack
C++ (Arduino Core) для STM32 F4xx, компилируется для встраиваемой платформы (жёсткие ограничения по памяти/времени).

## Code style
Специфика embedded C++: без `new`/`delete` (Zero Heap Allocation), без `std::function`/`std::unordered_map` в горячем пути (используется статическая LUT `CommandSlot handlersTable[256]`), обработчики прерываний (`ISR`) должны быть короткими и не блокирующими, `constexpr` для вычислений на этапе компиляции (например, `CrcTable`).

## Architecture and patterns
- Протокол COM-LINK-RT v2: заголовок 11 байт (`0xAA 0x55 version packetType serviceBits packetId dataLength crc`), CRC-16 CCITT-FALSE по табличному методу (полином `0x1021`).
- Диспетчеризация команд — O(1) через статический массив указателей на функции `handlersTable[256]`.
- Буфер парсера статического размера `BUFFER_SIZE=256`, payload передаётся по константной ссылке.
- Неблокирующий опрос HC-SR04 через аппаратные прерывания (`EchoISR`, флаг `echoReady`), формула `cm = (dur * 10) / 583`.
- `serviceBits` — модель publish/subscribe для телеметрии (`SUBSCRIBE 0x80`, `UNSUBSCRIBED 0x40`, `KEEP_ALIVE 0x20`, `UNSUBSCRIBE 0x10`), keep-alive таймаут 10000 мс.

## Dependencies on other parts of the system
- Общается по USB-Serial (115200 бод) с Orange Pi, на котором работает [izbushka-web-core](https://github.com/Emeteil/izbushka-web-core) через клиентскую библиотеку [com_link_rt_client](https://github.com/Emeteil/com_link_rt_client) (подключена туда как сабмодуль `com_link_rt`).
- Любое изменение формата пакета/`packetType`/`serviceBits` должно оставаться совместимым с парсером в `com_link_rt_client`.

## Review checklist
- Изменение структуры пакета (заголовок 11 байт, порядок полей, CRC) или таблицы `packetType` без соответствующего изменения в [com_link_rt_client](https://github.com/Emeteil/com_link_rt_client) — обязательно отметить необходимость синхронизации протокола.
- Появление `new`/`delete`/динамических контейнеров в пути парсинга/диспетчеризации — нарушает принцип Zero Heap Allocation.
- Блокирующие вызовы (`delay()`, `pulseIn()` и т.п.) в основном цикле или обработчиках — риск сбоя генерации ШИМ моторов.
- Длинная работа внутри `ISR`-обработчиков (например, `EchoISR`) — должны быть максимально короткими.
- Изменение пинов/адресов (I2C-адрес PCA9685, пины моторов/датчиков) без соответствия задокументированной распиновке — сверить с `board_config.h`.
- Переполнение статического буфера `BUFFER_SIZE=256` при увеличении payload.
- Изменение расчёта CRC (полином, порядок байт) — должно совпадать по обе стороны протокола.
