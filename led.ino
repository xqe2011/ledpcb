#include <WS2812.h>
#include "common.h"

#define COLOR_PER_LEDS 3
#define NUM_BYTES (LED_NUM * COLOR_PER_LEDS)

static uint8_t __xdata ledData[NUM_BYTES];
static uint16_t nowTime = 0; /* LED任务执行当前时间周期位置 */
static uint8_t cachedNowPointNumber[LED_NUM] = {0};
static const LED_TaskInfo* runningTask;
static uint32_t lastTickTime = 0;
static const LED_TaskInfo* nextTask = 0;

static uint8_t isIntervalIRSend = 0;
static uint32_t lastIntervalIRSendTime = 0;
static uint8_t intervalIRData[4] = { 0xA1, 0x01, 0x00, 0x00 };

extern volatile uint8_t ledClockCompensationFlag;

/* LED渐变转折点 */
extern const LED_TaskInfo task1;
extern const LED_TaskInfo irSendStart;
extern const LED_TaskInfo irSendStop;

static void LED_RunNextTick(const LED_TaskInfo* taskInfo)
{
    for (size_t i = 0; i < LED_NUM; i++) {
        const LED_RGBPoint* point = taskInfo->points[i];
        if (taskInfo->pointsLength[i] == 0 || cachedNowPointNumber[i] + 1 == taskInfo->pointsLength[i] || point[cachedNowPointNumber[i] + 1].time == 0) {
            /* 执行到最后一个转折点后面就不再执行这个LED的渐变了 */
            continue;
        }
        const LED_RGBPoint* nowPoint = &point[cachedNowPointNumber[i]];
        const LED_RGBPoint* nextPoint = &point[cachedNowPointNumber[i] + 1];
        /* 对第一个点的特殊处理 */
        if (cachedNowPointNumber[i] == 0 && nowPoint->time > nowTime) {
            set_pixel_for_GRB_LED(
                ledData,
                i,
                nowPoint->r,
                nowPoint->g,
                nowPoint->b
            );
        } else if (nowPoint->time <= nowTime && nextPoint->time > nowTime) {
            int16_t deltaTime = nextPoint->time - nowPoint->time;
            int16_t nowDeltaTime = nowTime - nowPoint->time;
            set_pixel_for_GRB_LED(
                ledData,
                i,
                (int16_t)nowPoint->r + (nextPoint->r - nowPoint->r) * nowDeltaTime / deltaTime,
                (int16_t)nowPoint->g + (nextPoint->g - nowPoint->g) * nowDeltaTime / deltaTime,
                (int16_t)nowPoint->b + (nextPoint->b - nowPoint->b) * nowDeltaTime / deltaTime
            );
        }
        if (taskInfo->pointsLength[i] != 1 && nextPoint->time == nowTime + 1) {
            /* 到达下一个转折点 */
            cachedNowPointNumber[i]++;
        }
    }
    /* 更新红外数据包包含的当前时间 */
    intervalIRData[2] = nowTime >> 8;
    intervalIRData[3] = nowTime & 0xFF;
    nowTime++;
    /* 循环执行 */
    if (nowTime == taskInfo->cycleTime) {
        if (nextTask != 0) {
            LED_ChangeTask(nextTask, 0);
            nextTask = 0;
            return;
        }
        nowTime = 0;
        memset(cachedNowPointNumber, 0, sizeof(cachedNowPointNumber));
    }
    ledClockCompensationFlag = 1;
    LED_PIN_SEND_FUNCTION(ledData, NUM_BYTES);
}

/**
 * 更换当前运行的LED任务
 * 
 * @param newTaskInfo 新任务信息
 * @param startTime 开始时间
 */
void LED_ChangeTask(const LED_TaskInfo* newTaskInfo, uint16_t startTime)
{
    runningTask = newTaskInfo;
    nowTime = startTime;
    /* 更新缓存转折点快速查找表 */
    for (size_t i = 0; i < LED_NUM; i++) {
        for (size_t a = 0; a < newTaskInfo->pointsLength[i]; a++) {
            if (runningTask->points[i][a].time < startTime) {
                cachedNowPointNumber[i] = a;
                break;
            }
        }
    }
}

/**
 * 更换下一次运行的LED任务
 * 
 * @param newTaskInfo 新任务信息
 */
void LED_ChangeTaskNext(const LED_TaskInfo* newTaskInfo)
{
    nextTask = newTaskInfo;
}

const LED_TaskInfo* LED_GetRunningTask()
{
    return runningTask;
}

void LED_SetIntervalIRSendEnable(uint8_t enable)
{
    isIntervalIRSend = enable;
    const LED_TaskInfo* runningTask = LED_GetRunningTask();
    if (isIntervalIRSend) {
        LED_ChangeTask(&irSendStart, 0);
    } else {
        LED_ChangeTask(&irSendStop, 0);
    }
    LED_ChangeTaskNext(runningTask);
}

void LED_Setup()
{
    pinMode(LED_PIN, OUTPUT);
    /* 默认任务 */
    runningTask = &task1;
}

void LED_Loop()
{
    uint32_t now = millis();
    if (now - lastTickTime > 10) {
        LED_RunNextTick(runningTask);
        lastTickTime = now;
    }
    if (isIntervalIRSend) {
        if (now - lastIntervalIRSendTime > 1000) {
            lastIntervalIRSendTime = now;
            IR_Send(intervalIRData);
        }
    }
}