#ifndef COMMON_H__
#define COMMON_H__

#include "config.h"

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint16_t time;
} LED_RGBPoint;

typedef struct {
    const LED_RGBPoint* points[LED_NUM];
    size_t pointsLength[LED_NUM];
    uint16_t cycleTime;
} LED_TaskInfo;

void LED_Setup();
void LED_Loop();
void LED_ChangeTask(__code const LED_TaskInfo* newTaskInfo, uint16_t startTime);
void LED_ChangeTaskNext(__code const LED_TaskInfo* newTaskInfo);
__code const LED_TaskInfo* LED_GetRunningTask();
void LED_SetIntervalIRSendEnable(uint8_t enable);

void Touch_Setup();
void Touch_Loop();

void Sound_Setup();
void Sound_Loop();
void Sound_Play(uint8_t number);

void IR_Setup();
void IR_Loop();
void IR_Send(__xdata uint8_t* data);

#endif