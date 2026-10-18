#pragma once

#define BOARD_STM32_FAXX

#define SERIAL_BAUD_RATE   115200
#define MAIN_LOOP_DELAY_MS 1

#define I2C1_SDA PB7
#define I2C1_SCL PB6

#define MOTOR1_ENA_PIN PE10
#define MOTOR1_IN1_PIN PE11
#define MOTOR1_IN2_PIN PE12
#define MOTOR1_MASK    0x01

#define MOTOR2_ENA_PIN PE13
#define MOTOR2_IN1_PIN PE14
#define MOTOR2_IN2_PIN PE15
#define MOTOR2_MASK    0x02

// 8 x HC-SR04 через два CD74HC4067: TRIG раздаётся одним мультиплексором, ECHO собирается другим,
// адрес S0-S2 общий. S3 и EN обоих мультиплексоров на GND.
#define ULTRASONIC_COUNT    8
#define ULTRASONIC_TRIG_PIN PC1
#define ULTRASONIC_ECHO_PIN PC2
#define ULTRASONIC_S0_PIN   PD12
#define ULTRASONIC_S1_PIN   PD13
#define ULTRASONIC_S2_PIN   PD14

// 8 слотов по 31 мс = 248 мс на полный круг, отправка раз в 250 мс (~4 Гц)
#define ULTRASONIC_SLOT_US          31000UL
#define ULTRASONIC_ECHO_TIMEOUT_US  26000UL
#define ULTRASONIC_SEND_INTERVAL_MS 250UL

// Скорость звука в воздухе при 20 °C. Ошибка ~0.17 % на каждый градус отличия.
#define ULTRASONIC_SOUND_SPEED_MM_S 343400UL

#define MAGNETOMETER_I2C_ADDRESS       0x2C
#define MAGNETOMETER_I2C_CLOCK_HZ      400000UL
#define MAGNETOMETER_READ_INTERVAL_MS  20UL
#define MAGNETOMETER_SEND_INTERVAL_MS  50UL
#define MAGNETOMETER_PROBE_INTERVAL_MS 1000UL

#define GPS_RX_PIN    PA3
#define GPS_TX_PIN    PA2
#define GPS_BAUD_RATE 9600UL

#define SUBSCRIPTION_KEEP_ALIVE_TIMEOUT_MS 10000
