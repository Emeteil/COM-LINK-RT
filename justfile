env := "black_f407ve"
lint_files := "$(find src -iname '*.cpp' -o -iname '*.h' | grep -v '\\.bak$'; echo com-link-RT.ino)"

# Проверка форматирования (clang-format --dry-run --Werror)
lint:
    clang-format --dry-run --Werror {{lint_files}}

# Автоформатирование по .clang-format
fmt:
    clang-format -i {{lint_files}}

build:
    pio run -e {{env}}

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
