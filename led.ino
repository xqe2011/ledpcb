#include <WS2812.h>
#include "common.h"

#define COLOR_PER_LEDS 3
#define NUM_BYTES (LED_NUM * COLOR_PER_LEDS)

static uint8_t __xdata ledData[NUM_BYTES];
static uint16_t nowTime = 0; /* LED任务执行当前时间周期位置 */
static uint8_t cachedNowPointNumber[LED_NUM] = {0};
static LED_TaskInfo* runningTask;
static uint32_t lastTickTime = 0;
static LED_TaskInfo* nextTask = 0;
static uint8_t taskMemory[256];
static uint16_t taskMemoryLength = 0;

static uint8_t isIntervalIRSend = 0;
static uint32_t lastIntervalIRSendTime = 0;
static uint8_t intervalIRData[4] = { 0xA1, 0x01, 0x00, 0x00 };

/* LED渐变转折点 */
LED_TaskInfo task1;
LED_TaskInfo irSendStart;
LED_TaskInfo irSendStop;

static void LED_RunNextTick(LED_TaskInfo* taskInfo)
{
    for (size_t i = 0; i < LED_NUM; i++) {
        LED_RGBPoint* point = taskInfo->points[i];
        if (taskInfo->pointsLength[i] == 0 || cachedNowPointNumber[i] + 1 == taskInfo->pointsLength[i] || point[cachedNowPointNumber[i] + 1].time == 0) {
            /* 执行到最后一个转折点后面就不再执行这个LED的渐变了 */
            continue;
        }
        LED_RGBPoint* nowPoint = &point[cachedNowPointNumber[i]];
        LED_RGBPoint* nextPoint = &point[cachedNowPointNumber[i] + 1];
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
    LED_PIN_SEND_FUNCTION(ledData, NUM_BYTES);
}

static void LED_AddPointToTask(LED_TaskInfo* taskInfo, uint8_t ledIndex, uint8_t r, uint8_t g, uint8_t b, uint16_t time)
{
    for (size_t i = 0; i < taskInfo->pointsLength[ledIndex]; i++) {
        if (taskInfo->points[ledIndex][i].time == 0) {
            taskInfo->points[ledIndex][i].r = (uint16_t)r * LED_BRIGHTNESS / 255;
            taskInfo->points[ledIndex][i].g = (uint16_t)g * LED_BRIGHTNESS / 255;
            taskInfo->points[ledIndex][i].b = (uint16_t)b * LED_BRIGHTNESS / 255;
            taskInfo->points[ledIndex][i].time = time;
            return;
        }
    }
    USBSerial_println("Cannot add point to led: do not has enough buffer.");
}

static void LED_MallocTaskBuffer(LED_TaskInfo* taskInfo, uint8_t index, size_t length)
{
    size_t size = length * sizeof(LED_RGBPoint);
    if (taskMemoryLength + size > sizeof(taskMemory)) {
        USBSerial_println("Cannot malloc task buffer: do not has enough buffer.");
        return;
    }
    uint8_t* result = taskMemory + taskMemoryLength;
    taskMemoryLength += size;
    taskInfo->points[index] = (LED_RGBPoint*)result;
    taskInfo->pointsLength[index] = length;
}

/**
 * 更换当前运行的LED任务
 * 
 * @param newTaskInfo 新任务信息
 * @param startTime 开始时间
 */
void LED_ChangeTask(LED_TaskInfo* newTaskInfo, uint16_t startTime)
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
void LED_ChangeTaskNext(LED_TaskInfo* newTaskInfo)
{
    nextTask = newTaskInfo;
}

LED_TaskInfo* LED_GetRunningTask()
{
    return runningTask;
}

void LED_SetIntervalIRSendEanble(uint8_t enable)
{
    isIntervalIRSend = enable;
    LED_TaskInfo* runningTask = LED_GetRunningTask();
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
    /* LED任务，每个LED对应的转折点必须从时间小到大顺序添加 */
    /* TASK1 */
    task1.cycleTime = 300;
    LED_MallocTaskBuffer(&task1, 0, 4);
    LED_AddPointToTask(&task1, 0, 255, 0, 0, 0);
    LED_AddPointToTask(&task1, 0, 0, 255, 0, 100);
    LED_AddPointToTask(&task1, 0, 0, 0, 255, 200);
    LED_AddPointToTask(&task1, 0, 255, 0, 0, 300);
    /* 红外发送开始任务 */
    irSendStart.cycleTime = 300;
    LED_MallocTaskBuffer(&irSendStart, 0, 1);
    LED_AddPointToTask(&irSendStart, 0, 255, 0, 0, 300);
    /* 红外发送开始任务 */
    irSendStop.cycleTime = 300;
    LED_MallocTaskBuffer(&irSendStop, 0, 2);
    LED_AddPointToTask(&irSendStop, 0, 255, 0, 0, 0);
    LED_AddPointToTask(&irSendStop, 0, 0, 0, 0, 300);
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