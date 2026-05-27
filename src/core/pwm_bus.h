#pragma once
#include <Adafruit_PWMServoDriver.h>
#include "board_config.h"

namespace PwmBus
{
    extern Adafruit_PWMServoDriver driver;

    bool Init();
    bool IsReady();
}