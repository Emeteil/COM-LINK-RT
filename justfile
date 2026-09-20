env := "black_f407ve"
test_env := "native"
lint_files := "$(find src test -iname '*.cpp' -o -iname '*.h' | grep -v '\\.bak$'; echo com-link-RT.ino)"

# Проверка форматирования (clang-format --dry-run --Werror)
lint:
    clang-format --dry-run --Werror {{lint_files}}

# Автоформатирование по .clang-format
fmt:
    clang-format -i {{lint_files}}

build:
    pio run -e {{env}}

# Нативные unit-тесты (GoogleTest, железо не нужно)
test:
    pio test -e {{test_env}}

coverage: test

# Только ядро протокола
test-core:
    pio test -e {{test_env}} -f test_core

# Только команды
test-commands:
    pio test -e {{test_env}} -f test_commands

# Один набор: just test-filter test_millis
test-filter name:
    pio test -e {{test_env}} -f {{name}}

# Прошивка по SWD через ST-Link (OpenOCD, без STM32CubeProgrammer)
upload:
    pio run -e {{env}} -t upload

# Прошивка через встроенный USB DFU-бутлоадер (BOOT0 -> 1)
upload-dfu:
    pio run -e {{env}}_dfu -t upload

# Прошивка через USB-UART переходник (BOOT0 -> 1)
upload-serial port="COM9":
    pio run -e {{env}}_serial -t upload --upload-port={{port}}

monitor:
    pio device monitor -b 115200

size:
    pio run -e {{env}} -t size

clean:
    pio run -e {{env}} -t clean
