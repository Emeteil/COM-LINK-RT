#include "servo.h"
#include "../core/globals.h"
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

namespace CommandServo
{
    Adafruit_PWMServoDriver pca;
    
    ServoTask taskQueue[MAX_TASKS];
    ChannelState channelStates[MAX_CHANNELS];
    uint8_t taskCount;
    bool initialized;
    
    uint16_t AngleToPulse(uint16_t angle)
    {
        return map(angle, 0, 180, SERVO_MIN, SERVO_MAX);
    }
    
    void SetServoPosition(uint8_t channel, uint16_t angle)
    {
        if (!initialized || channel >= MAX_CHANNELS)
            return;
        
        uint16_t pulse = AngleToPulse(angle);
        pca.setPWM(channel, 0, pulse);
        
        channelStates[channel].currentAngle = angle;
        channelStates[channel].isDefined = true;
    }
    
    void ClearQueue()
    {
        for (uint8_t i = 0; i < taskCount; i++)
        {
            taskQueue[i].isActive = false;
        }
        taskCount = 0;
    }
    
    bool AddTask(const ServoCommand& cmd, TaskPriority priority)
    {
        if (taskCount >= MAX_TASKS)
            return false;
        
        ServoTask newTask;
        newTask.command = cmd;
        newTask.priority = priority;
        newTask.isActive = true;
        newTask.startTime = millis();
        newTask.lastStepTime = newTask.startTime;
        
        uint8_t channel = cmd.channel;
        if (channelStates[channel].isDefined)
        {
            newTask.startAngle = channelStates[channel].currentAngle;
        }
        else
        {
            newTask.startAngle = 90;
            channelStates[channel].currentAngle = 90;
            channelStates[channel].isDefined = true;
        }
        
        newTask.currentAngle = newTask.startAngle;
        
        if (priority == PRIORITY_HIGH)
        {
            ClearQueue();
            
            taskQueue[0] = newTask;
            taskCount = 1;
        }
        else
        {
            taskQueue[taskCount] = newTask;
            taskCount++;
        }
        
        return true;
    }
    
    void RemoveTask(uint8_t index)
    {
        if (index >= taskCount)
            return;
        
        for (uint8_t i = index; i < taskCount - 1; i++)
        {
            taskQueue[i] = taskQueue[i + 1];
        }
        
        taskCount--;
    }
    
    void ProcessImmediateMove(ServoTask& task)
    {
        SetServoPosition(task.command.channel, task.command.targetAngle);
        task.isActive = false;
    }
    
    void ProcessSmoothMove(ServoTask& task)
    {
        unsigned long currentTime = millis();
        
        if (currentTime - task.lastStepTime < task.command.stepDelay)
            return;
        
        int16_t direction = (task.command.targetAngle > task.currentAngle) ? 1 : -1;
        task.currentAngle += direction;
        
        SetServoPosition(task.command.channel, task.currentAngle);
        
        task.lastStepTime = currentTime;
        
        task.isActive = (task.currentAngle != task.command.targetAngle);
    }
    
    void ProcessTask(uint8_t index)
    {
        if (!taskQueue[index].isActive)
            return;
        
        uint8_t channel = taskQueue[index].command.channel;
        
        if (!channelStates[channel].isDefined && taskQueue[index].command.moveType != MOVE_IMMEDIATE)
            taskQueue[index].command.moveType = MOVE_IMMEDIATE;
        
        switch (taskQueue[index].command.moveType)
        {
            case MOVE_IMMEDIATE:
                ProcessImmediateMove(taskQueue[index]);
                break;

            case MOVE_SMOOTH_LOW:
            case MOVE_SMOOTH_HIGH:
#ifdef SERVO_DISABLE_SMOOTH
                ProcessImmediateMove(taskQueue[index]);
#else
                ProcessSmoothMove(taskQueue[index]);
#endif
                break;
        }
        
        if (!taskQueue[index].isActive)
            RemoveTask(index);
    }
    
    void SendResponse(uint16_t packetId)
    {
        uint8_t txBuffer[64];
        uint16_t length;
        
        protocol.parser.CreatePacket(
            PROTOCOL_VERSION,
            PACKET_TYPE_RESPONSE,
            EMPTY_SERVICE_BITS,
            packetId,
            nullptr,
            0,
            txBuffer,
            length
        );
        
        protocol.SendPacket(txBuffer, length, packetId);
    }
    
    void Init()
    {
        Wire.setSDA(I2C1_SDA);
        Wire.setSCL(I2C1_SCL);
        Wire.begin();

        pca = Adafruit_PWMServoDriver(PWM_SERVO_ADDRESS);
        
        if (!pca.begin())
        {
            initialized = false;
            return;
        }
        
        pca.setPWMFreq(SERVO_FREQ);
        
        for (uint8_t i = 0; i < MAX_CHANNELS; i++)
        {
            channelStates[i].currentAngle = 90;
            channelStates[i].isDefined = false;
        }
        
        for (uint8_t i = 0; i < MAX_TASKS; i++)
        {
            taskQueue[i].isActive = false;
        }
        
        taskCount = 0;
        initialized = true;
    }
    
    void Handler(ComLinkRTProtocol::PacketHeader header, uint8_t* data)
    {
        if (!initialized || header.dataLength != sizeof(ServoCommand))
            return;
        
        ServoCommand cmd;
        memcpy(&cmd, data, sizeof(ServoCommand));
        
        if (cmd.channel >= MAX_CHANNELS || cmd.targetAngle > 180)
            return;
        
        TaskPriority priority;
        switch (cmd.moveType)
        {
            case MOVE_IMMEDIATE:
                priority = PRIORITY_HIGH;
                break;
            case MOVE_SMOOTH_HIGH:
                priority = PRIORITY_HIGH;
                break;
            case MOVE_SMOOTH_LOW:
                priority = PRIORITY_LOW;
                break;
            default:
                return;
        }
        
        if (AddTask(cmd, priority) && header.packetId != ZERO_PACKET_ID)
        {
            SendResponse(header.packetId);
        }
    }
    
    void Processor()
    {
        if (!initialized)
            return;
        
        for (uint8_t i = 0; i < taskCount; i++)
        {
            ProcessTask(i);
        }
    }
}