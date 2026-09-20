#include "pwm_bus.h"
#include <Arduino.h>
#include <Wire.h>

namespace PwmBus
{
    Adafruit_PWMServoDriver driver(PWM_SERVO_ADDRESS);

    static bool initialized = false;

    bool Init()
    {
        if (initialized)
            return true;

        Wire.setSDA(I2C1_SDA);
        Wire.setSCL(I2C1_SCL);
        Wire.begin();

        if (!driver.begin())
            return false;

        driver.setPWMFreq(PWM_SERVO_FREQ_HZ);
        initialized = true;
        return true;
    }

    bool IsReady()
    {
        return initialized;
    }

    void Reset()
    {
        initialized = false;
    }
}