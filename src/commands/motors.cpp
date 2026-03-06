#include "motors.h"
#include "../core/globals.h"
#include <Arduino.h>

namespace CommandMotors
{
    MotorState MotorStates[2];
    
    void SetMotorDirection(uint8_t motorIndex, MotorDirection direction)
    {
        if (motorIndex >= 2)
            return;
        
        MotorStates[motorIndex].direction = direction;
        
        switch (motorIndex)
        {
            case 0:
                switch (direction)
                {
                    case DIRECTION_FORWARD:
                        digitalWrite(MOTOR1_IN1_PIN, HIGH);
                        digitalWrite(MOTOR1_IN2_PIN, LOW);
                        break;
                    case DIRECTION_BACKWARD:
                        digitalWrite(MOTOR1_IN1_PIN, LOW);
                        digitalWrite(MOTOR1_IN2_PIN, HIGH);
                        break;
                    case DIRECTION_STOP:
                        digitalWrite(MOTOR1_IN1_PIN, LOW);
                        digitalWrite(MOTOR1_IN2_PIN, LOW);
                        break;
                    case DIRECTION_BRAKE:
                        digitalWrite(MOTOR1_IN1_PIN, HIGH);
                        digitalWrite(MOTOR1_IN2_PIN, HIGH);
                        break;
                }
                break;
                
            case 1:
                switch (direction)
                {
                    case DIRECTION_FORWARD:
                        digitalWrite(MOTOR2_IN1_PIN, HIGH);
                        digitalWrite(MOTOR2_IN2_PIN, LOW);
                        break;
                    case DIRECTION_BACKWARD:
                        digitalWrite(MOTOR2_IN1_PIN, LOW);
                        digitalWrite(MOTOR2_IN2_PIN, HIGH);
                        break;
                    case DIRECTION_STOP:
                        digitalWrite(MOTOR2_IN1_PIN, LOW);
                        digitalWrite(MOTOR2_IN2_PIN, LOW);
                        break;
                    case DIRECTION_BRAKE:
                        digitalWrite(MOTOR2_IN1_PIN, HIGH);
                        digitalWrite(MOTOR2_IN2_PIN, HIGH);
                        break;
                }
                break;
        }
        
        MotorStates[motorIndex].isActive = (direction != DIRECTION_STOP);
    }
    
    void SetMotorSpeed(uint8_t motorIndex, uint8_t speed)
    {
        if (motorIndex >= 2) return;
        
        MotorStates[motorIndex].speed = speed;
        
        switch (motorIndex)
        {
            case 0:
                analogWrite(MOTOR1_ENA_PIN, speed);
                break;
            case 1:
                analogWrite(MOTOR2_ENA_PIN, speed);
                break;
        }
    }
    
    void StopAllMotors()
    {
        for (uint8_t i = 0; i < 2; i++)
        {
            SetMotorDirection(i, DIRECTION_STOP);
            SetMotorSpeed(i, 0);
        }
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
            nullptr, 0,
            txBuffer,
            length
        );
        
        protocol.SendPacket(txBuffer, length, packetId);
    }
    
    void ProcessMotorCommand(const MotorCommand& cmd)
    {
        switch (cmd.commandType)
        {
            case COMMAND_SET_SPEED:
                if (cmd.motorMask & MOTOR1_MASK)
                    SetMotorSpeed(0, cmd.speed1);
                
                if (cmd.motorMask & MOTOR2_MASK)
                    SetMotorSpeed(1, cmd.speed2);
                break;
                
            case COMMAND_SET_DIRECTION:
                if (cmd.motorMask & MOTOR1_MASK)
                    SetMotorDirection(0, static_cast<MotorDirection>(cmd.direction1));
                
                if (cmd.motorMask & MOTOR2_MASK)
                    SetMotorDirection(1, static_cast<MotorDirection>(cmd.direction2));
                break;
                
            case COMMAND_SET_BOTH:
                if (cmd.motorMask & MOTOR1_MASK)
                {
                    SetMotorDirection(0, static_cast<MotorDirection>(cmd.direction1));
                    SetMotorSpeed(0, cmd.speed1);
                }
                if (cmd.motorMask & MOTOR2_MASK)
                {
                    SetMotorDirection(1, static_cast<MotorDirection>(cmd.direction2));
                    SetMotorSpeed(1, cmd.speed2);
                }
                break;
                
            case COMMAND_STOP_ALL:
                StopAllMotors();
                break;
                
            case COMMAND_SET_DIFFERENTIAL:
                SetMotorSpeed(0, cmd.speed1);
                SetMotorSpeed(1, cmd.speed2);
                
                if (cmd.direction1 == DIRECTION_FORWARD && cmd.direction2 == DIRECTION_FORWARD)
                {
                    SetMotorDirection(0, DIRECTION_FORWARD);
                    SetMotorDirection(1, DIRECTION_FORWARD);
                }
                else if (cmd.direction1 == DIRECTION_BACKWARD && cmd.direction2 == DIRECTION_BACKWARD)
                {
                    SetMotorDirection(0, DIRECTION_BACKWARD);
                    SetMotorDirection(1, DIRECTION_BACKWARD);
                }
                else if (cmd.direction1 == DIRECTION_FORWARD && cmd.direction2 == DIRECTION_BACKWARD)
                {
                    SetMotorDirection(0, DIRECTION_FORWARD);
                    SetMotorDirection(1, DIRECTION_BACKWARD);
                }
                else if (cmd.direction1 == DIRECTION_BACKWARD && cmd.direction2 == DIRECTION_FORWARD)
                {
                    SetMotorDirection(0, DIRECTION_BACKWARD);
                    SetMotorDirection(1, DIRECTION_FORWARD);
                }
                break;
        }
    }
    
    void Init()
    {
        pinMode(MOTOR1_ENA_PIN, OUTPUT);
        pinMode(MOTOR1_IN1_PIN, OUTPUT);
        pinMode(MOTOR1_IN2_PIN, OUTPUT);
        
        pinMode(MOTOR2_ENA_PIN, OUTPUT);
        pinMode(MOTOR2_IN1_PIN, OUTPUT);
        pinMode(MOTOR2_IN2_PIN, OUTPUT);
        
        StopAllMotors();
        
        for (uint8_t i = 0; i < 2; i++)
        {
            MotorStates[i].direction = DIRECTION_STOP;
            MotorStates[i].speed = 0;
            MotorStates[i].isActive = false;
        }
    }
    
    void Handler(ComLinkRTProtocol::PacketHeader header, uint8_t* data)
    {
        if (header.dataLength != sizeof(MotorCommand))
            return;
        
        MotorCommand cmd;
        memcpy(&cmd, data, sizeof(MotorCommand));
        
        ProcessMotorCommand(cmd);
        
        if (header.packetId != ZERO_PACKET_ID)
            SendResponse(header.packetId);
    }
}